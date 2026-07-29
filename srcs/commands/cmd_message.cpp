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
    std::string line = clientPrefix(client) + " " + command + " " + target + " :" + text + "\r\n";

    // a target starting with # is a channel, anything else is a nickname
    if (!target.empty() && target[0] == '#') {
        std::map<std::string, Channel>::iterator it = _channels.find(target);
        if (it == _channels.end()) {
            if (!notice)
                sendNumeric(client, "403", target + " :No such channel");
            return;
        }
        if (!it->second.members.count(client.fd)) {
            if (!notice)
                sendNumeric(client, "404", target + " :Cannot send to channel");
            return;
        }
        // to everyone in the channel but the one who sent it
        sendToChannel(target, line, client.fd);
    } else {
        Client* dest = findClientByNick(target);
        if (!dest) {
            if (!notice)
                sendNumeric(client, "401", target + " :No such nick");
            return;
        }
        sendToClient(dest->fd, line);
    }
}

void Server::cmdPrivmsg(Client& client, const Message& msg) {
    deliverMessage(client, msg, "PRIVMSG", false);
}

void Server::cmdNotice(Client& client, const Message& msg) {
    deliverMessage(client, msg, "NOTICE", true);
}

void Server::cmdQuit(Client& client, const Message& msg) {
    std::string reason = msg.params.empty() ? "Client quit" : msg.params[0];
    std::string line = clientPrefix(client) + " QUIT :" + reason + "\r\n";

    // send the quit to every user who shares a channel with them, but only once per
    // person, so someone who is in two of their channels doesn't get it twice
    std::set<int> told;
    for (std::set<std::string>::iterator ch = client.channels.begin(); ch != client.channels.end(); ++ch) {
        std::map<std::string, Channel>::iterator it = _channels.find(*ch);
        if (it == _channels.end())
            continue;
        for (std::set<int>::iterator m = it->second.members.begin(); m != it->second.members.end(); ++m) {
            if (*m != client.fd && told.insert(*m).second)
                sendToClient(*m, line);
        }
    }

    // take them out of every channel, deleting the ones left empty, then drop the connection
    for (std::set<std::string>::iterator ch = client.channels.begin(); ch != client.channels.end(); ++ch) {
        std::map<std::string, Channel>::iterator it = _channels.find(*ch);
        if (it == _channels.end())
            continue;
        it->second.members.erase(client.fd);
        it->second.operators.erase(client.fd);
        it->second.invited.erase(client.fd);
        if (it->second.members.empty())
            _channels.erase(it);
    }
    disconnect(client.fd);
}
