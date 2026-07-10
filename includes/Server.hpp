#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <map>
#include "Client.hpp"
#include "Channel.hpp"
#include "Message.hpp"

class Server {
public:
    Server(int port, const std::string& password);
    ~Server();

    void run();

private:
    int         _listen_fd;
    bool        _stop;      // flipped by the SIGINT handler, checked each loop tick
    int         _port;
    std::string _password;

    std::map<int, Client>          _clients;
    std::map<std::string, Channel> _channels;

    void acceptClient();
    void recvFromClient(int fd);
    void flushClient(int fd);
    void disconnect(int fd);
    void dispatch(Client& client, const Message& msg);
    void sendToClient(int fd, const std::string& msg);
    void sendToChannel(const std::string& channel, const std::string& msg, int except_fd);
};

#endif
