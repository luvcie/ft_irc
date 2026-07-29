#include "Server.hpp"

void Server::cmdPrivmsg(Client& client, const Message& msg) {
    if (!client.registered) {
        sendNumeric(client, "451", ":You have not registered");
        return;
    }
    if (msg.params.empty()) {
        sendNumeric(client, "411", ":No recipient given (PRIVMSG)");
        return;
    }
    if (msg.params.size() < 2) {
        sendNumeric(client, "412", ":No text to send");
        return;
    }

    const std::string& target = msg.params[0];
    const std::string& text = msg.params[1];
    std::string line = clientPrefix(client) + " PRIVMSG " + target + " :" + text + "\r\n";

    // a target starting with # is a channel, anything else is a nickname
    if (!target.empty() && target[0] == '#') {
        std::map<std::string, Channel>::iterator it = _channels.find(target);
        if (it == _channels.end()) {
            sendNumeric(client, "403", target + " :No such channel");
            return;
        }
        if (!it->second.members.count(client.fd)) {
            sendNumeric(client, "404", target + " :Cannot send to channel");
            return;
        }
        // to everyone in the channel but the one who sent it
        sendToChannel(target, line, client.fd);
    } else {
        Client* dest = findClientByNick(target);
        if (!dest) {
            sendNumeric(client, "401", target + " :No such nick");
            return;
        }
        sendToClient(dest->fd, line);
    }
}
