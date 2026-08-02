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
#include <arpa/inet.h>

// signal handlers can't reach members, so the stop flag lives here
static volatile sig_atomic_t g_stop = 0;

// the protocol caps a line at 512 bytes, this leaves plenty of room on top of it.
// what it stops is a client that sends forever without ever closing a line, which
// would grow recv_buf until the machine runs out of memory
static const std::string::size_type MAX_PENDING_LINE = 4096;

static void on_sigint(int)
{
	g_stop = 1;
}

Server::Server(int port, const std::string &password)
	: _listen_fd(-1), _port(port), _password(password)
{
	_handlers["PASS"] = &Server::cmdPass;
	_handlers["NICK"] = &Server::cmdNick;
	_handlers["USER"] = &Server::cmdUser;
	_handlers["PING"] = &Server::cmdPing;
	_handlers["JOIN"] = &Server::cmdJoin;
	_handlers["PART"] = &Server::cmdPart;
	_handlers["PRIVMSG"] = &Server::cmdPrivmsg;
	_handlers["NOTICE"] = &Server::cmdNotice;
	_handlers["QUIT"] = &Server::cmdQuit;
	_handlers["TOPIC"] = &Server::cmdTopic;
	_handlers["KICK"] = &Server::cmdKick;
	_handlers["MODE"] = &Server::cmdMode;
	_handlers["CAP"] = &Server::cmdCap;
	_handlers["INVITE"] = &Server::cmdInvite;
}

Server::~Server()
{
}

void Server::run()
{
	_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (_listen_fd < 0)
	{
		std::cerr << "Error: socket failed" << std::endl;
		std::exit(1);
	}
	int yes = 1;
	if (setsockopt(_listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
	{
		std::cerr << "Error: setsockopt failed" << std::endl;
		std::exit(1);
	}

	sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(_port);
	if (bind(_listen_fd, (sockaddr *)&addr, sizeof(addr)) < 0)
	{
		// ports below 1024 are privileged: binding one without root fails here.
		// checking the port number, not errno, tells the two cases apart
		if (_port < 1024)
			std::cerr << "Error: bind failed, port " << _port
					  << " needs root or is already in use\n"
					  << "Privileged ports: [1-1023]\n"
					  << "Unprivileged ports: [1024-65535]" << std::endl;
		else
			std::cerr << "Error: bind failed (port already in use?)" << std::endl;
		std::exit(1);
	}
	if (listen(_listen_fd, SOMAXCONN) < 0)
	{
		std::cerr << "Error: listen failed" << std::endl;
		std::exit(1);
	}
	if (fcntl(_listen_fd, F_SETFL, O_NONBLOCK) < 0)
	{
		std::cerr << "Error: fcntl failed" << std::endl;
		std::exit(1);
	}

	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, on_sigint);

	std::cout << "ircserv listening on port " << _port << std::endl;

	while (!g_stop)
	{
		// rebuilt from scratch every tick, keeps it trivially in sync with _clients
		std::vector<pollfd> pfds;
		pollfd lp;
		lp.fd = _listen_fd;
		lp.events = POLLIN;
		lp.revents = 0;
		pfds.push_back(lp);
		for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
		{
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

		for (size_t i = 1; i < pfds.size(); ++i)
		{
			int fd = pfds[i].fd;
			if (pfds[i].revents & (POLLHUP | POLLERR))
			{
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

	std::cout << "\nircserv shutting down" << std::endl;
	for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
		close(it->first);
	_clients.clear();
	close(_listen_fd);
}

void Server::acceptClient()
{
	sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);
	int fd = accept(_listen_fd, (sockaddr *)&addr, &addrlen);
	// a failed accept is normal (a connection dropped before accept), so just skip it
	if (fd < 0)
		return;
	if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
	{
		std::cerr << "Error: fcntl failed" << std::endl;
		close(fd);
		return;
	}
	// keep the client's ip, it goes in the nick!user@host part of their messages
	char ip[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
	Client client(fd);
	client.host = ip;
	_clients.insert(std::make_pair(fd, client));
}

void Server::recvFromClient(int fd)
{
	char buf[512];
	int n = recv(fd, buf, sizeof(buf), 0);
	if (n <= 0)
	{
		disconnect(fd);
		return;
	}
	std::map<int, Client>::iterator it = _clients.find(fd);
	if (it == _clients.end())
		return;
	Client &client = it->second;
	client.recv_buf.append(buf, n);
	while (client.recv_buf.find('\n') != std::string::npos)
	{
		std::string::size_type pos = client.recv_buf.find('\n');
		std::string line = client.recv_buf.substr(0, pos);
		client.recv_buf.erase(0, pos + 1);
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);
		dispatch(client, parse(line));
		// stop before using it again because a command like quit can disconnect the
		// client during the loop, and then the client reference would be dead
		if (_clients.find(fd) == _clients.end())
			return;
	}
	// whatever is left over is an unfinished line. past this size it is not a slow
	// client, it is one that will never send a newline, so the connection goes
	if (client.recv_buf.size() > MAX_PENDING_LINE)
		disconnect(fd, "Input line too long");
}

void Server::flushClient(int fd)
{
	std::map<int, Client>::iterator it = _clients.find(fd);
	if (it == _clients.end())
		return;
	Client &client = it->second;
	if (client.send_buf.empty())
		return;
	int n = send(fd, client.send_buf.c_str(), client.send_buf.size(), 0);
	if (n > 0)
		client.send_buf.erase(0, n);
}

// a client leaving, whether they typed QUIT or their socket just died. both paths
// come through here so the cleanup only has to exist once
void Server::disconnect(int fd, const std::string &reason)
{
	std::map<int, Client>::iterator it = _clients.find(fd);
	if (it == _clients.end())
	{
		close(fd);
		return;
	}
	Client &client = it->second;

	// tell everyone who shares a channel with them, but only once per person, so
	// somebody in two of their channels doesn't get the same line twice
	if (client.registered)
	{
		std::string line = clientPrefix(client) + " QUIT :" + reason + "\r\n";
		std::set<int> told;
		for (std::set<std::string>::iterator ch = client.channels.begin(); ch != client.channels.end(); ++ch)
		{
			std::map<std::string, Channel>::iterator chan_it = _channels.find(*ch);
			if (chan_it == _channels.end())
				continue;
			std::set<int> &members = chan_it->second.members;
			for (std::set<int>::iterator m = members.begin(); m != members.end(); ++m)
			{
				if (*m != fd && told.insert(*m).second)
					sendToClient(*m, line);
			}
		}
	}

	// only now take them out, a second loop so nobody is removed before being told.
	// leaving the fd behind matters more than it looks: fds get recycled, and the
	// next client to connect could inherit an operator flag that was never theirs
	for (std::set<std::string>::iterator ch = client.channels.begin(); ch != client.channels.end(); ++ch)
	{
		std::map<std::string, Channel>::iterator chan_it = _channels.find(*ch);
		if (chan_it == _channels.end())
			continue;
		chan_it->second.members.erase(fd);
		chan_it->second.operators.erase(fd);
		chan_it->second.invited.erase(fd);
		if (chan_it->second.members.empty())
			_channels.erase(chan_it);
	}

	// last of all: erasing the client destroys the reference and its channel list
	close(fd);
	_clients.erase(fd);
}

void Server::dispatch(Client &client, const Message &msg)
{
	if (msg.command.empty())
		return;
	std::map<std::string, Handler>::iterator it = _handlers.find(msg.command);
	if (it == _handlers.end())
	{
		sendNumeric(client, "421", msg.command + " :Unknown command");
		return;
	}
	// handler points at one of our own methods, ->* calls the method through the pointer
	Handler handler = it->second;
	(this->*handler)(client, msg);
}

void Server::sendToClient(int fd, const std::string &msg)
{
	std::map<int, Client>::iterator it = _clients.find(fd);
	if (it != _clients.end())
		it->second.send_buf += msg;
}

void Server::sendNumeric(Client &client, const std::string &code, const std::string &params)
{
	// a client with no nick yet is shown as * in the reply
	std::string target = client.nick.empty() ? "*" : client.nick;
	sendToClient(client.fd, ":" SERVER_NAME " " + code + " " + target + " " + params + "\r\n");
}

void Server::sendToChannel(const std::string &channel, const std::string &msg, int except_fd)
{
	std::map<std::string, Channel>::iterator it = _channels.find(channel);
	if (it == _channels.end())
		return;
	Channel &chan = it->second;
	for (std::set<int>::iterator mem_it = chan.members.begin(); mem_it != chan.members.end(); ++mem_it)
	{
		if (*mem_it != except_fd)
			sendToClient(*mem_it, msg);
	}
}

Client* Server::findClientByNick(const std::string& nick)
{
	std::string want = ircLower(nick);
	for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		if (ircLower(it->second.nick) == want)
			return &(it->second);
	}
	return NULL;
}
