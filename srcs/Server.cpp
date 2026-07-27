#include "Server.hpp"

#include <vector>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <csignal>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>

// signal handlers can't reach members, so the stop flag lives here
static volatile sig_atomic_t g_stop = 0;

static void on_sigint(int) {
    g_stop = 1;
}

Server::Server(int port, const std::string& password)
    : _listen_fd(-1), _port(port), _password(password)
{
    _handlers["PASS"] = &Server::cmdPass;
    _handlers["NICK"] = &Server::cmdNick;
    _handlers["USER"] = &Server::cmdUser;
}

Server::~Server()
{}

void Server::run() {
    _listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_listen_fd < 0) {
        std::cerr << "Error: socket failed" << std::endl;
        std::exit(1);
    }
    int yes = 1;
    if (setsockopt(_listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        std::cerr << "Error: setsockopt failed" << std::endl;
        std::exit(1);
    }

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(_port);
    if (bind(_listen_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Error: bind failed (port already in use?)" << std::endl;
        std::exit(1);
    }
    if (listen(_listen_fd, SOMAXCONN) < 0) {
        std::cerr << "Error: listen failed" << std::endl;
        std::exit(1);
    }
    if (fcntl(_listen_fd, F_SETFL, O_NONBLOCK) < 0) {
        std::cerr << "Error: fcntl failed" << std::endl;
        std::exit(1);
    }

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, on_sigint);

    while (!g_stop) {
        // rebuilt from scratch every tick, keeps it trivially in sync with _clients
        std::vector<pollfd> pfds;
        pollfd lp;
        lp.fd = _listen_fd;
        lp.events = POLLIN;
        lp.revents = 0;
        pfds.push_back(lp);
        for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
            pollfd p;
            p.fd = it->first;
            p.events = POLLIN;
            if (!it->second.send_buf.empty())
                p.events |= POLLOUT;
            p.revents = 0;
            pfds.push_back(p);
        }

        if (poll(&pfds[0], pfds.size(), -1) < 0)
            continue; // interrupted by a signal, the while condition decides

        if (pfds[0].revents & POLLIN)
            acceptClient();

        for (size_t i = 1; i < pfds.size(); ++i) {
            int fd = pfds[i].fd;
            if (pfds[i].revents & (POLLHUP | POLLERR)) {
                disconnect(fd);
                continue;
            }
            if (pfds[i].revents & POLLIN)
                recvFromClient(fd);
            // recvFromClient may have disconnected them, check before writing
            if ((pfds[i].revents & POLLOUT) && _clients.count(fd))
                flushClient(fd);
        }
    }

    for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
        close(it->first);
    _clients.clear();
    close(_listen_fd);
}

// TODO for Pablo: acepta la conexión, ponla en no bloqueante con fcntl y mete el Client en _clients
void Server::acceptClient()
{
	sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);
	int fd = accept(_listen_fd, (sockaddr*)&addr, &addrlen);
	if (fd < 0) {
		std::cerr << "Error: accept failed" << std::endl;
		return;
	}
	if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
		std::cerr << "Error: fcntl failed" << std::endl;
		close(fd);
		return;
	}
	_clients.insert(std::make_pair(fd, Client(fd)));
}

// TODO for Pablo: un solo recv a un buffer local, 0 o menos significa que el cliente se ha ido (disconnect).
// Si no, añade a recv_buf y ve sacando las líneas completas, valen tanto \r\n como \n a secas.
// Una línea a medias se queda en recv_buf para la próxima. Cada línea completa va a dispatch(client, parse(line)).
void Server::recvFromClient(int fd) {
    (void)fd;
}

// TODO for Pablo: un send con lo que haya, y quita de send_buf solo lo que el kernel haya aceptado
void Server::flushClient(int fd) {
    (void)fd;
}

// TODO for Pablo: cierra el fd y borra el cliente de _clients
void Server::disconnect(int fd) {
    (void)fd;
}

void Server::dispatch(Client& client, const Message& msg) {
    if (msg.command.empty())
        return;
    std::map<std::string, Handler>::iterator it = _handlers.find(msg.command);
    if (it == _handlers.end())
        return;
    // handler points at one of our own methods, ->* calls the method through the pointer
    Handler handler = it->second;
    (this->*handler)(client, msg);
}

void Server::sendToClient(int fd, const std::string& msg) {
    std::map<int, Client>::iterator it = _clients.find(fd);
    if (it != _clients.end())
        it->second.send_buf += msg;
}

void Server::sendNumeric(Client& client, const std::string& code, const std::string& params) {
    // a client with no nick yet is shown as * in the reply
    std::string target = client.nick.empty() ? "*" : client.nick;
    sendToClient(client.fd, ":" SERVER_NAME " " + code + " " + target + " " + params + "\r\n");
}

// TODO for Pablo: recorre los miembros del canal llamando a sendToClient para cada uno,
// saltándote except_fd (el que envía)
void Server::sendToChannel(const std::string& channel, const std::string& msg, int except_fd) {
    (void)channel;
    (void)msg;
    (void)except_fd;
}
