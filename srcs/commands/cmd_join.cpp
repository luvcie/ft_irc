#include "Server.hpp"

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
    const std::string& name = msg.params[0];
    if (!isValidChannelName(name)) {
        sendNumeric(client, "403", name + " :No such channel");
        return;
    }

    std::map<std::string, Channel>::iterator it = _channels.find(name);
    bool created = (it == _channels.end());
    if (created) {
        Channel fresh;
        fresh.name = name;
        it = _channels.insert(std::make_pair(name, fresh)).first;
    }
    Channel& chan = it->second;
    if (chan.members.count(client.fd))
        return;

    chan.members.insert(client.fd);
    client.channels.insert(name);
    // whoever creates the channel gets to run it
    if (created)
        chan.operators.insert(client.fd);

    // let everyone in the channel know, the one joining included
    std::string line = ":" + client.nick + "!" + client.user + "@" + client.host + " JOIN " + name + "\r\n";
    sendToChannel(name, line, -1);
}
