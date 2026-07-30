#include "Server.hpp"

// PRIVMSG and NOTICE do the exact same thing, the only difference is that NOTICE
// never answers with an error. that's on purpose so two bots talking to each other
// can't bounce error replies back and forth forever
void Server::deliverMessage(Client& client, const Message& msg, const std::string& command, bool notice) {
    if (!client.registered) {
        if (!notice)
            sendNumeric(client, "451", ":You have not registered");
        return;
    }
    if (msg.params.empty()) {
        if (!notice)
            sendNumeric(client, "411", ":No recipient given (" + command + ")");
        return;
    }
    if (msg.params.size() < 2) {
        if (!notice)
            sendNumeric(client, "412", ":No text to send");
        return;
    }

    const std::string& target = msg.params[0];
    const std::string& text = msg.params[1];

    // a target starting with # is a channel, anything else is a nickname
    if (!target.empty() && target[0] == '#') {
        std::string name = ircLower(target);
        std::map<std::string, Channel>::iterator it = _channels.find(name);
        if (it == _channels.end()) {
            if (!notice)
                sendNumeric(client, "403", name + " :No such channel");
            return;
        }
        if (!it->second.members.count(client.fd)) {
            if (!notice)
                sendNumeric(client, "404", name + " :Cannot send to channel");
            return;
        }
        std::string line = clientPrefix(client) + " " + command + " " + name + " :" + text + "\r\n";
        // to everyone in the channel but the one who sent it
        sendToChannel(name, line, client.fd);
    } else {
        Client* dest = findClientByNick(target);
        if (!dest) {
            if (!notice)
                sendNumeric(client, "401", target + " :No such nick");
            return;
        }
        std::string line = clientPrefix(client) + " " + command + " " + target + " :" + text + "\r\n";
        sendToClient(dest->fd, line);
    }
}

void Server::cmdPrivmsg(Client& client, const Message& msg) {
    deliverMessage(client, msg, "PRIVMSG", false);
}

void Server::cmdNotice(Client& client, const Message& msg) {
    deliverMessage(client, msg, "NOTICE", true);
}

// the announcing and the cleanup both live in disconnect(), because a socket that
// just dies has to do exactly the same work as somebody typing QUIT. all this adds
// is the reason the user gave
void Server::cmdQuit(Client& client, const Message& msg) {
    disconnect(client.fd, msg.params.empty() ? "Client quit" : msg.params[0]);
}
