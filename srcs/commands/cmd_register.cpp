#include "Server.hpp"

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
