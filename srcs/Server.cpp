#include "Server.hpp"

Server::Server(int port, const std::string& password)
    : _listen_fd(-1), _stop(false), _port(port), _password(password)
{}

Server::~Server()
{}

void Server::run()
{}

void Server::acceptClient()
{}

void Server::recvFromClient(int fd) {
    (void)fd;
}

void Server::flushClient(int fd) {
    (void)fd;
}

void Server::disconnect(int fd) {
    (void)fd;
}

void Server::dispatch(Client& client, const Message& msg) {
    (void)client;
    (void)msg;
}

void Server::sendToClient(int fd, const std::string& msg) {
    (void)fd;
    (void)msg;
}

void Server::sendToChannel(const std::string& channel, const std::string& msg, int except_fd) {
    (void)channel;
    (void)msg;
    (void)except_fd;
}
