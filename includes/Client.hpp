#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <set>
#include <ctime>

class Client {
public:
    Client(int fd);

    int                   fd;
    std::string           nick;
    std::string           user;
    std::string           host;
    bool                  has_pass;
    bool                  registered;
    time_t                connected_at; // when the socket connected, for the registration timeout
    std::string           recv_buf;  // data arrives in chunks, lines get cut from here
    std::string           send_buf;  // replies queue here, the loop drains it when the socket is ready
    std::set<std::string> channels;
};

#endif
