#include <iostream>
#include <cstdlib>
#include <string>
#include "Server.hpp"

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <port> <password>" << std::endl;
        return 1;
    }

    std::string port_str(argv[1]);
    if (port_str.empty() || port_str.size() > 5
        || port_str.find_first_not_of("0123456789") != std::string::npos) {
        std::cerr << "Error: port must be a number between 1 and 65535" << std::endl;
        return 1;
    }
    int port = std::atoi(port_str.c_str());
    if (port < 1 || port > 65535) {
        std::cerr << "Error: port must be between 1 and 65535" << std::endl;
        return 1;
    }

    std::string password(argv[2]);
    if (password.empty()) {
        std::cerr << "Error: password cannot be empty" << std::endl;
        return 1;
    }

    Server server(port, password);
    server.run();
    return 0;
}
