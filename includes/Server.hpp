#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <map>
#include "Client.hpp"
#include "Channel.hpp"
#include "Message.hpp"

#define SERVER_NAME "ircserv"
#define SERVER_VERSION "1.0"
// 003 asks when the server was created, the build date is as good an answer as any
#define SERVER_CREATED __DATE__ " " __TIME__
// 004 advertises these, so keep them in step with what cmdMode actually accepts
#define SERVER_CHANMODES "itkol"

// keep this under the fd limit (1024 by default), otherwise accept() fails and poll
// keeps waking us up for a connection we can never take
#define MAX_CLIENTS           512u
#define MAX_CHANNELS          1024u
#define MAX_CHANNELS_PER_USER 20u
#define MAX_CHANNEL_USERS     256u
// a frozen client accumulates whatever its channels say. big enough to survive a
// flood, still capped so one dead client can't eat all the ram
#define MAX_SENDQ             67108864u
#define REG_TIMEOUT           60       // seconds to finish PASS/NICK/USER before the socket is dropped

// pastel 256-color codes for the log tags
#define CLR_GREEN  "151"
#define CLR_RED    "210"
#define CLR_CYAN   "159"
#define CLR_PURPLE "183"
#define CLR_BLUE   "117"

std::string ircLower(const std::string& s);
std::string logTag(const std::string& name, const char* color);

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
    void logStatus(const std::string& event);
    std::string clientPrefix(const Client& client);
	Client* findClientByNick(const std::string& nick);

    void cmdPass(Client& client, const Message& msg);
    void cmdNick(Client& client, const Message& msg);
    void cmdUser(Client& client, const Message& msg);
    void cmdPing(Client& client, const Message& msg);
    void cmdJoin(Client& client, const Message& msg);
    void joinChannel(Client& client, const std::string& name, const std::string& key);
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
	void cmdWho(Client& client, const Message& msg);
};

#endif
