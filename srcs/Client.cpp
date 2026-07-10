#include "Client.hpp"

Client::Client(int fd)
    : fd(fd), has_pass(false), registered(false)
{}
