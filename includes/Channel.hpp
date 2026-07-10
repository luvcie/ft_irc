#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <set>
#include <cstddef>

class Channel {
public:
    Channel();

    std::string   name;
    std::string   topic;
    std::string   key;
    std::set<int> members;
    std::set<int> operators;
    bool          invite_only;
    bool          topic_locked;
    std::size_t   user_limit;   // 0 means no limit
    std::set<int> invited;
};

#endif
