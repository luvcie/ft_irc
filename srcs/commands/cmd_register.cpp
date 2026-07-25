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

static std::string toLower(const std::string& s) {
    std::string out = s;
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = std::tolower((unsigned char)out[i]);
    return out;
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
    // registration checks this at the end, here we only remember if it matched
    client.has_pass = (msg.params[0] == _password);
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
        if (it->first != client.fd && toLower(it->second.nick) == toLower(nick)) {
            sendNumeric(client, "433", nick + " :Nickname is already in use");
            return;
        }
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
}
