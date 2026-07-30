#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <map>
#include "Client.hpp"
#include "Channel.hpp"
#include "Message.hpp"

#define SERVER_NAME "ircserv"

std::string ircLower(const std::string& s);

class Server {
public:
    Server(int port, const std::string& password);
    ~Server();

    void run();

private:
    typedef void (Server::*Handler)(Client&, const Message&);

    int         _listen_fd;
    int         _port;
    std::string _password;

    std::map<int, Client>          _clients;
    std::map<std::string, Channel> _channels;
    std::map<std::string, Handler> _handlers;

    void acceptClient();
    void recvFromClient(int fd);
    void flushClient(int fd);
    void disconnect(int fd, const std::string& reason = "Connection closed");
    void dispatch(Client& client, const Message& msg);
    void sendToClient(int fd, const std::string& msg);
    void sendToChannel(const std::string& channel, const std::string& msg, int except_fd);
    void sendNumeric(Client& client, const std::string& code, const std::string& params);
    std::string clientPrefix(const Client& client);
	Client* findClientByNick(const std::string& nick);

    void cmdPass(Client& client, const Message& msg);
    void cmdNick(Client& client, const Message& msg);
    void cmdUser(Client& client, const Message& msg);
    void cmdPing(Client& client, const Message& msg);
    void cmdJoin(Client& client, const Message& msg);
    void cmdPart(Client& client, const Message& msg);
    void cmdPrivmsg(Client& client, const Message& msg);
    void cmdNotice(Client& client, const Message& msg);
    void cmdQuit(Client& client, const Message& msg);
    void deliverMessage(Client& client, const Message& msg, const std::string& command, bool notice);
    void tryRegister(Client& client);
	void cmdTopic(Client& client, const Message& msg);
	void cmdKick(Client& client, const Message& msg);
	void cmdMode(Client& client, const Message& msg);
	void cmdCap(Client& client, const Message& msg);
	void cmdInvite(Client& client, const Message& msg);
};

#endif
