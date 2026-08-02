*This project has been created as part of the 42 curriculum by lucpardo, pramos-c.*

# ft_irc

An IRC server written in C++98, built around a single `poll()` event loop.

![42 Project](https://img.shields.io/badge/42-Project-blue)

## Description

ft_irc is an IRC server that follows the classic model: clients connect over
TCP, register with a password, a nickname and a username, then join channels
and talk to each other in real time. It answers a real IRC client, so a client
like irssi can connect, register, join channels and chat with no extra setup.

The whole server runs in a single thread. One call to `poll()` watches the
listening socket and every connected client at the same time, so no connection
ever blocks another. Every socket is non-blocking, reads and writes only happen
when `poll()` reports the socket is ready, and each client carries its own
receive and send buffer. That way a command that arrives in pieces, or a client
that reads slowly, is handled without holding up the rest of the loop.

Channels support the usual operator toolkit: an operator can set the topic,
kick members, invite people, and change channel modes (invite only, topic
lock, key, operator privilege, and a user limit).

## Instructions

### Requirements

Linux, a C++ compiler with C++98 support, and GNU make.

### Build

```bash
make        # build ircserv
make clean  # remove object files
make fclean # remove object files and the executable
make re     # fclean followed by make
```

### Run

The program takes a port and a connection password:

```bash
./ircserv <port> <password>
```

For example:

```bash
./ircserv 6667 mypassword
```

The port has to be a number between 1 and 65535, and the password cannot be
empty. Ports below 1024 need root.

### Connecting

Any IRC client works. With irssi, running on the same machine as the server:

```
/connect 127.0.0.1 6667 mypassword
/join #general
/msg #general hello
```

For raw testing, `nc` sends commands by hand:

```bash
nc 127.0.0.1 6667
PASS mypassword
NICK alice
USER alice 0 * :Alice
JOIN #general
PRIVMSG #general :hello
```

## Commands

| Command | Purpose |
|---------|---------|
| `PASS` | Send the connection password |
| `NICK` | Set or change the nickname |
| `USER` | Set the username, completes registration |
| `PING` | Keepalive, the server answers with `PONG` |
| `JOIN` | Join a channel |
| `PART` | Leave a channel |
| `PRIVMSG` | Send a message to a user or a channel |
| `NOTICE` | Like `PRIVMSG`, but never triggers an automatic reply |
| `QUIT` | Disconnect from the server |
| `TOPIC` | Read or set a channel topic |
| `KICK` | Remove a user from a channel (operator) |
| `INVITE` | Invite a user to a channel (operator) |
| `MODE` | Read or change channel modes (operator) |
| `CAP` | Capability negotiation, kept minimal so clients connect cleanly |

## Channel modes

| Mode | Meaning |
|------|---------|
| `i` | Invite only, only invited users can join |
| `t` | Only operators can change the topic |
| `k` | Channel key, a password needed to join |
| `o` | Give or take operator privilege |
| `l` | User limit, caps how many can be in the channel |

## Resources

- RFC 1459, Internet Relay Chat Protocol: https://datatracker.ietf.org/doc/html/rfc1459
- RFC 2812, IRC Client Protocol: https://datatracker.ietf.org/doc/html/rfc2812
- Modern IRC documentation: https://modern.ircdocs.horse/
- Beej's Guide to Network Programming: https://beej.us/guide/bgnet/
- irssi: https://irssi.org/

AI was used as a learning aid to understand the IRC protocol (RFC 1459 and
2812), the single poll() event loop, non-blocking socket handling, other
random questions, and an overall understanding of how servers work in an
interactive conversation because chatbots are cool to talk to for learning
about different subjects :). All project code was written and reviewed by the
authors!
