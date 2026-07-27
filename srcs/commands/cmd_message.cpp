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
    // TODO: in the next commit send it to a nick or to a channel
}
