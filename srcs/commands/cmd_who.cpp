#include "Server.hpp"

// minimal WHO so irssi can fill its user list: lists a channel's members
void Server::cmdWho(Client& client, const Message& msg) {
    if (!msg.params.empty()) {
        std::string name = ircLower(msg.params[0]);
        std::map<std::string, Channel>::iterator it = _channels.find(name);
        if (it != _channels.end()) {
            std::set<int>& members = it->second.members;
            for (std::set<int>::iterator m = members.begin(); m != members.end(); ++m) {
                Client& c = _clients.find(*m)->second;
                std::string flag = it->second.operators.count(*m) ? "H@" : "H";
                sendNumeric(client, "352", name + " " + c.user + " " + c.host + " " SERVER_NAME " "
                            + c.nick + " " + flag + " :0 " + c.nick);
            }
        }
    }
    std::string who = msg.params.empty() ? std::string("*") : msg.params[0];
    sendNumeric(client, "315", who + " :End of /WHO list");
}
