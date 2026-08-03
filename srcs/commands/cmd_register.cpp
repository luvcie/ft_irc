#include "Server.hpp"
#include <cctype>

// can also use these characters in a nickname
static const std::string NICK_SPECIAL = "[]\\`_^{|}";

static bool isValidNick(const std::string& nick) {
    if (nick.empty())
        return false;
    if (!std::isalpha((unsigned char)nick[0]) && NICK_SPECIAL.find(nick[0]) == std::string::npos)
        return false;
    for (size_t i = 1; i < nick.size(); ++i) {
        char c = nick[i];
        if (!std::isalnum((unsigned char)c) && NICK_SPECIAL.find(c) == std::string::npos && c != '-')
            return false;
    }
    return true;
}

// irc compares nicks without caring about case, and on top of a-z it also treats
// []\~ as the same as {}|^ (a leftover from irc being scandinavian, rfc 2812 section 2.2)
std::string ircLower(const std::string& s) {
    std::string out = s;
    for (size_t i = 0; i < out.size(); ++i) {
        char c = out[i];
        if (c >= 'A' && c <= 'Z')
            out[i] = c + 32;
        else if (c == '[')
            out[i] = '{';
        else if (c == ']')
            out[i] = '}';
        else if (c == '\\')
            out[i] = '|';
        else if (c == '~')
            out[i] = '^';
    }
    return out;
}

// real clients open with CAP LS to negotiate ircv3 extensions before registering.
// we support none, so we answer with an empty list and they carry on. without this
// handler the command would reach the 421 above and show as an error in the client
void Server::cmdCap(Client &client, const Message &msg)
{
	if (!msg.params.empty() && msg.params[0] == "LS")
		sendToClient(client.fd, ":" SERVER_NAME " CAP * LS :\r\n");
}

void Server::cmdPass(Client& client, const Message& msg) {
    if (client.registered) {
        sendNumeric(client, "462", ":Already registered, can't do it again");
        return;
    }
    if (msg.params.empty()) {
        sendNumeric(client, "461", "PASS :Not enough parameters");
        return;
    }
    // registration checks this at the end, this line only records whether it matched
    client.has_pass = (msg.params[0] == _password);
    // NICK and USER both try to register when done, so PASS should too. if PASS
    // comes last the password would just sit here and the client stays stuck
    tryRegister(client);
}

void Server::cmdNick(Client& client, const Message& msg) {
    if (msg.params.empty()) {
        sendNumeric(client, "431", ":No nickname given");
        return;
    }
    const std::string& nick = msg.params[0];
    if (!isValidNick(nick)) {
        sendNumeric(client, "432", nick + " :Erroneous nickname");
        return;
    }
    // two people can't share the same nickname, example: Luvcie and luvcie are seen as the same
    for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if (it->first != client.fd && ircLower(it->second.nick) == ircLower(nick)) {
            sendNumeric(client, "433", nick + " :Nickname is already in use");
            return;
        }
    }
    // asking for the nick you already have is a no-op, but changing only the case is
    // a real change and does get announced
    if (client.nick == nick)
        return;
    // somebody already registered is known to other people, so the change has to be
    // announced to every channel they are in, plus to themselves as a confirmation.
    // the prefix carries the OLD nick, that is how the others know who to rename, so
    // the line has to be built before the assignment below
    if (client.registered) {
        std::string line = clientPrefix(client) + " NICK :" + nick + "\r\n";
        std::set<int> told;
        told.insert(client.fd);
        sendToClient(client.fd, line);
        for (std::set<std::string>::iterator ch = client.channels.begin(); ch != client.channels.end(); ++ch) {
            std::map<std::string, Channel>::iterator it = _channels.find(*ch);
            if (it == _channels.end())
                continue;
            std::set<int>& members = it->second.members;
            for (std::set<int>::iterator m = members.begin(); m != members.end(); ++m) {
                if (told.insert(*m).second)
                    sendToClient(*m, line);
            }
        }
        logStatus(logTag("nick", CLR_PURPLE) + " " + client.nick + " is now " + nick);
    }
    client.nick = nick;
    tryRegister(client);
}

void Server::cmdUser(Client& client, const Message& msg) {
    if (client.registered) {
        sendNumeric(client, "462", ":Already registered, can't do it again");
        return;
    }
    // USER comes with 4 params: username, hostname, servername and the real name
    if (msg.params.size() < 4) {
        sendNumeric(client, "461", "USER :Not enough parameters");
        return;
    }
    client.user = msg.params[0];
    tryRegister(client);
}

void Server::tryRegister(Client& client) {
    if (client.registered)
        return;
    // need both the nick and the user before registration can finish
    if (client.nick.empty() || client.user.empty())
        return;
    // this is the moment a wrong or missing password gets rejected
    if (!client.has_pass) {
        sendNumeric(client, "464", ":Password incorrect");
        return;
    }
    client.registered = true;
    sendNumeric(client, "001", ":Welcome to the network, " + client.nick);
    logStatus(logTag("*", CLR_CYAN) + " register   " + client.nick);
}

void Server::cmdPing(Client& client, const Message& msg) {
    if (msg.params.empty()) {
        sendNumeric(client, "409", ":No origin specified");
        return;
    }
    // reply PONG but keep the same value client sent with msg.params[0], so client knows it answers their ping
    sendToClient(client.fd, ":" SERVER_NAME " PONG " SERVER_NAME " :" + msg.params[0] + "\r\n");
}
