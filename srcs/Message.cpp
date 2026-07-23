#include "Message.hpp"
#include <cctype>

static std::string toUpper(const std::string& s) {
    std::string out = s;
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = std::toupper(static_cast<unsigned char>(out[i]));
    return out;
}

Message parse(const std::string& line) {
    Message msg;
    size_t i = 0;

    // clients aren't supposed to send a prefix, but if one shows up
    // (it starts with ':') skip past it so it doesn't get read as the command
    if (i < line.size() && line[i] == ':') {
        while (i < line.size() && line[i] != ' ')
            ++i;
        while (i < line.size() && line[i] == ' ')
            ++i;
    }

    size_t start = i;
    while (i < line.size() && line[i] != ' ')
        ++i;
    msg.command = toUpper(line.substr(start, i - start));

    while (i < line.size()) {
        while (i < line.size() && line[i] == ' ')
            ++i;
        if (i >= line.size())
            break;
        // a param starting with ':' takes the rest of the line
        if (line[i] == ':') {
            msg.params.push_back(line.substr(i + 1));
            break;
        }
        start = i;
        while (i < line.size() && line[i] != ' ')
            ++i;
        msg.params.push_back(line.substr(start, i - start));
    }

    return msg;
}
