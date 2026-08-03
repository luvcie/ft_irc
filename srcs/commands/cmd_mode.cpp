#include "Server.hpp"
#include <sstream>

// turns a decimal string into a positive limit. C++98 has no std::stoi, so the
// conversion goes through a stream: reading a second character tells us whether
// anything was left over ("12abc" would otherwise pass as 12)
static bool parseLimit(const std::string &text, std::size_t &out)
{
  std::istringstream iss(text);
  long value = 0;
  char leftover;

  if (!(iss >> value))
    return false;
  if (iss >> leftover)
    return false;
  // read into a long and not a size_t: "-5" would wrap into a huge positive
  if (value <= 0)
    return false;
  out = static_cast<std::size_t>(value);
  return true;
}

// adds a comma before the next phrase once the string already has one
static void addPhrase(std::string &desc, const std::string &phrase)
{
  if (!desc.empty())
    desc += ", ";
  desc += phrase;
}

// writes the mode letter, the +/- only when the sign changes, and a plain-words
// gloss for the log. param is the nick for o, the value for l, empty otherwise
static void appendMode(std::string &modes, std::string &desc, char &last_sign,
                       bool adding, char letter, const std::string &param)
{
  char want = adding ? '+' : '-';
  if (want != last_sign)
  {
    modes += want;
    last_sign = want;
  }
  modes += letter;

  switch (letter)
  {
  case 'i': addPhrase(desc, adding ? "invite-only on" : "invite-only off"); break;
  case 't': addPhrase(desc, adding ? "topic-lock on" : "topic-lock off"); break;
  case 'k': addPhrase(desc, adding ? "key set" : "key removed"); break;
  case 'l': addPhrase(desc, adding ? "limit " + param : std::string("limit removed")); break;
  case 'o': addPhrase(desc, (adding ? "gave operator to " : "took operator from ") + param); break;
  }
}

void Server::cmdMode(Client &client, const Message &msg)
{
  if (!client.registered)
  {
    sendNumeric(client, "451", ":You have not registered");
    return;
  }
  if (msg.params.empty())
  {
    sendNumeric(client, "461", "MODE :Not enough parameters");
    return;
  }

  // lowercase like every other channel command, so MODE #Test finds #test
  std::string name = ircLower(msg.params[0]);

  // only channel modes are implemented, user modes are out of the subject. but
  // asking for your own modes is normal (irssi does it on connect), so answer
  // that with an empty mode list instead of an error. anyone else still gets 502
  if (name.empty() || name[0] != '#')
  {
    if (name == ircLower(client.nick))
      sendNumeric(client, "221", "+");
    else
      sendNumeric(client, "502", ":Cannot change mode for other users");
    return;
  }
  std::map<std::string, Channel>::iterator it = _channels.find(name);
  if (it == _channels.end())
  {
    sendNumeric(client, "403", name + " :No such channel");
    return;
  }

  Channel &chan = it->second;

  if (!chan.members.count(client.fd))
  {
    sendNumeric(client, "442", name + " :You're not on that channel");
    return;
  }

  // info request: MODE #chan with no second parameter shows the current modes and their values
  // a query needs no operator privilege, so it comes before the 482 check ex: MODE #chantest
  if (msg.params.size() == 1)
  {
    std::string modes = "+";
    std::string modes_params = "";

    if (chan.invite_only)
      modes += 'i';
    if (chan.topic_locked)
      modes += 't';
    // k and l also carry a value, so they feed a second half of the reply
    if (!chan.key.empty())
    {
      modes += 'k';
      modes_params += " " + chan.key;
    }
    if (chan.user_limit > 0)
    {
      std::ostringstream oss;
      oss << chan.user_limit;
      modes += 'l';
      modes_params += " " + oss.str();
    }
    sendNumeric(client, "324", name + " " + modes + modes_params);
    return;
  }

    // list modes are asked for, not set: irssi sends MODE #chan b right after joining
  // to read the ban list. we keep no such list, so the answer is just the end marker.
  // this goes before the operator check because reading a list is not privileged
  if (msg.params[1] == "b")
  {
    sendNumeric(client, "368", name + " :End of channel ban list");
    return;
  }

  if (!chan.operators.count(client.fd))
  {
    sendNumeric(client, "482", name + " :You're not channel operator");
    return;
  }

  const std::string &modestring = msg.params[1]; // "+ot" -> example
  bool adding = true;
  size_t param_index = 2;
  std::string applied_modes = "";  // fills up grouped, like "+it-l"
  std::string applied_params = ""; // fills up as " victim" or " key" or " 10"
  std::string applied_desc = "";   // plain-words gloss for the log, like "invite-only off"
  char last_sign = 0;              // the last +/- written, so signs only repeat when they change

  for (size_t i = 0; i < modestring.size(); ++i)
  {
    char c = modestring[i];

    if (c == '+')
    {
      adding = true;
      continue;
    }
    if (c == '-')
    {
      adding = false;
      continue;
    }

    if (c == 'o')
    {
      if (param_index >= msg.params.size())
        continue; // the nick is missing: skip this mode, no error
      const std::string &nick = msg.params[param_index];
      param_index = param_index + 1;

      Client *target = findClientByNick(nick);
      if (!target)
      {
        sendNumeric(client, "401", nick + " :No such nick");
        continue;
      }
      if (!chan.members.count(target->fd))
      {
        sendNumeric(client, "441", nick + " " + name + " :They aren't on that channel");
        continue;
      }

      if (adding)
        chan.operators.insert(target->fd);
      else
        chan.operators.erase(target->fd);

      appendMode(applied_modes, applied_desc, last_sign, adding, 'o', nick);
      applied_params += " " + nick;
    }
    else if (c == 't')
    {
      chan.topic_locked = adding;
      appendMode(applied_modes, applied_desc, last_sign, adding, 't', "");
    }
    else if (c == 'i')
    {
      chan.invite_only = adding;
      appendMode(applied_modes, applied_desc, last_sign, adding, 'i', "");
    }
    else if (c == 'k')
    {
      // only +k carries a parameter, -k just clears the key
      if (adding)
      {
        if (param_index >= msg.params.size())
          continue; // the key is missing: skip this mode, no error
        const std::string &key = msg.params[param_index];
        param_index = param_index + 1;

        if (key.empty())
          continue;

        chan.key = key;
        appendMode(applied_modes, applied_desc, last_sign, adding, 'k', "");
        applied_params += " " + key;
      }
      else
      {
        chan.key = "";
        appendMode(applied_modes, applied_desc, last_sign, adding, 'k', "");
      }
    }
    else if (c == 'l')
    {
      // same asymmetry as k: only +l carries a parameter
      if (adding)
      {
        if (param_index >= msg.params.size())
          continue;
        const std::string &text = msg.params[param_index];
        param_index = param_index + 1;

        std::size_t limit;
        if (!parseLimit(text, limit))
          continue; // "+l abc" and "+l -5" are ignored, never a crash

        chan.user_limit = limit;
        appendMode(applied_modes, applied_desc, last_sign, adding, 'l', text);
        applied_params += " " + text;
      }
      else
      {
        chan.user_limit = 0; // 0 means no limit
        appendMode(applied_modes, applied_desc, last_sign, adding, 'l', "");
      }
    }
    else
      sendNumeric(client, "472", std::string(1, c) + " :is unknown mode char to me");
  }

  // only what really got applied is announced, not what was asked for
  if (!applied_modes.empty())
  {
    std::string line = clientPrefix(client) + " MODE " + name + " "
                       + applied_modes + applied_params + "\r\n";
    sendToChannel(name, line, -1);
    logStatus(logTag("mode", CLR_BLUE) + " " + client.nick + " " + applied_modes + applied_params + " on " + name + " (" + applied_desc + ")");
  }
}
