#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <string>
#include <vector>

struct Message {
    std::string              command;
    std::vector<std::string> params;
};

Message parse(const std::string& line);

#endif
