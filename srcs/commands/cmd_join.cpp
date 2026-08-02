#include "Server.hpp"

std::string Server::clientPrefix(const Client& client) {
    return ":" + client.nick + "!" + client.user + "@" + client.host;
}

static bool isValidChannelName(const std::string& name) {
    if (name.size() < 2 || name[0] != '#')
        return false;
    for (size_t i = 1; i < name.size(); ++i) {
        char c = name[i];
        if (c == ' ' || c == ',' || c == ':' || c == 7)
            return false;
    }
    return true;
}

void Server::cmdJoin(Client& client, const Message& msg) {
    if (!client.registered) {
        sendNumeric(client, "451", ":You have not registered");
        return;
    }
    if (msg.params.empty()) {
        sendNumeric(client, "461", "JOIN :Not enough parameters");
        return;
    }
    // channel names are case-insensitive too (same rfc rule as nicks), so lowercase the
    // name and use that everywhere, that way #Test and #test end up as one channel
    std::string name = ircLower(msg.params[0]);
    if (!isValidChannelName(name)) {
        sendNumeric(client, "403", name + " :No such channel");
        return;
    }
    // cap how many channels one client can be in, but let them rejoin one they
    // are already on, that path is a no-op below
    if (!client.channels.count(name) && client.channels.size() >= MAX_CHANNELS_PER_USER) {
        sendNumeric(client, "405", name + " :You have joined too many channels");
        return;
    }

    std::map<std::string, Channel>::iterator it = _channels.find(name);
    bool created = (it == _channels.end());
    // refuse to create a brand new channel past the server-wide limit
    if (created && _channels.size() >= MAX_CHANNELS) {
        sendToClient(client.fd, ":" SERVER_NAME " NOTICE " + client.nick + " :Server channel limit reached\r\n");
        return;
    }
    if (created) {
        Channel fresh;
        fresh.name = name;
        it = _channels.insert(std::make_pair(name, fresh)).first;
    }
    Channel& chan = it->second;
    if (chan.members.count(client.fd))
        return;
    if (chan.invite_only && !chan.invited.count(client.fd))
    {
        sendNumeric(client, "473", name + " :Cannot join channel (+i)");
        return;
    }
    if (!chan.key.empty() && (msg.params.size() < 2 || msg.params[1] != chan.key))
    {
        sendNumeric(client, "475", name + " :Cannot join channel (+k)");
        return;
    }
    if (chan.user_limit > 0 && chan.members.size() >= chan.user_limit)
    {
        sendNumeric(client, "471", name + " :Cannot join channel (+l)");
        return;
    }
    // a hard ceiling on channel size, on top of whatever +l an operator set
    if (chan.members.size() >= MAX_CHANNEL_USERS)
    {
        sendNumeric(client, "471", name + " :Cannot join channel (channel is full)");
        return;
    }
    chan.members.insert(client.fd);
    client.channels.insert(name);
    chan.invited.erase(client.fd);
    // whoever creates the channel gets to run it
    if (created)
        chan.operators.insert(client.fd);

    // let everyone in the channel know, the one joining included
    std::string line = clientPrefix(client) + " JOIN " + name + "\r\n";
    sendToChannel(name, line, -1);

    // send the topic to the user who just joined, or say there isn't one yet
    if (chan.topic.empty())
        sendNumeric(client, "331", name + " :No topic is set");
    else
        sendNumeric(client, "332", name + " :" + chan.topic);

    // and the list of the users who are already in the channel, operators marked with a @
    std::string names;
    for (std::set<int>::iterator it = chan.members.begin(); it != chan.members.end(); ++it) {
        if (!names.empty())
            names += " ";
        if (chan.operators.count(*it))
            names += "@";
        names += _clients.find(*it)->second.nick;
    }
    sendNumeric(client, "353", "= " + name + " :" + names);
    sendNumeric(client, "366", name + " :End of /NAMES list");
    logStatus(logTag("join", CLR_GREEN) + " " + client.nick + " " + name);
}

void Server::cmdPart(Client& client, const Message& msg) {
    if (!client.registered) {
        sendNumeric(client, "451", ":You have not registered");
        return;
    }
    if (msg.params.empty()) {
        sendNumeric(client, "461", "PART :Not enough parameters");
        return;
    }
    std::string name = ircLower(msg.params[0]);
    std::map<std::string, Channel>::iterator it = _channels.find(name);
    if (it == _channels.end()) {
        sendNumeric(client, "403", name + " :No such channel");
        return;
    }
    Channel& chan = it->second;
    if (!chan.members.count(client.fd)) {
        sendNumeric(client, "442", name + " :You're not on that channel");
        return;
    }

    std::string line = clientPrefix(client) + " PART " + name;
    if (msg.params.size() > 1)
        line += " :" + msg.params[1];
    line += "\r\n";
    // send it before taking them out of the channel, so the one leaving gets it too
    sendToChannel(name, line, -1);

    chan.members.erase(client.fd);
    chan.operators.erase(client.fd);
    chan.invited.erase(client.fd);
    client.channels.erase(name);
    // a channel with nobody left in it just disappears
    if (chan.members.empty())
        _channels.erase(it);
    logStatus(logTag("part", CLR_RED) + " " + client.nick + " " + name);
}

