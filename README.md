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

The third column is what you type into `nc`, exactly as written.

| Command | Purpose | netcat |
|---------|---------|--------|
| `PASS` | Send the connection password | `PASS mypassword` |
| `NICK` | Set or change the nickname | `NICK alice` |
| `USER` | Set the username, completes registration | `USER alice 0 * :Alice Smith` |
| `PING` | Keepalive, the server answers with `PONG` | `PING hello` |
| `JOIN` | Join one or more channels | `JOIN #general`<br>`JOIN #general hunter2` (channel has a key)<br>`JOIN #general,#off-topic hunter2,secret` |
| `PART` | Leave a channel | `PART #general`<br>`PART #general :bye everyone` |
| `PRIVMSG` | Send a message to a user or a channel | `PRIVMSG #general :hello`<br>`PRIVMSG bob :hi there` |
| `NOTICE` | Like `PRIVMSG`, but never triggers an automatic reply | `NOTICE #general :heads up` |
| `QUIT` | Disconnect from the server | `QUIT`<br>`QUIT :see you` |
| `TOPIC` | Read or set a channel topic | `TOPIC #general` (read)<br>`TOPIC #general :new topic` (set) |
| `KICK` | Remove a user from a channel (operator) | `KICK #general bob`<br>`KICK #general bob :spamming` |
| `INVITE` | Invite a user to a channel (operator) | `INVITE bob #general` |
| `MODE` | Read or change channel modes (operator) | `MODE #general` (read)<br>`MODE #general +k hunter2` (set) |
| `WHO` | List the members of a channel | `WHO #general` |
| `CAP` | Capability negotiation, kept minimal so clients connect cleanly | `CAP LS` |

Three things that trip people up typing these by hand:

- The `:` marks the **last** parameter, and everything after it counts as one
  piece even with spaces in it. `PRIVMSG #general hello there` only delivers
  `hello`, while `PRIVMSG #general :hello there` delivers the whole sentence.
- `INVITE` takes the nickname first and the channel second, which is the
  opposite order from `KICK`.
- `JOIN` takes a whole list at once, and the keys line up with it in the same order:
  `JOIN #general,#off-topic hunter2,secret`. irssi rejoins every channel in one
  `JOIN` after a reconnect, so this one is not optional. `PART` and `PRIVMSG` still
  take a single channel or target.

## Channel modes

| Mode | Meaning | netcat |
|------|---------|--------|
| `i` | Invite only, only invited users can join | `MODE #general +i`<br>`MODE #general -i` |
| `t` | Only operators can change the topic | `MODE #general +t`<br>`MODE #general -t` |
| `k` | Channel key, a password needed to join | `MODE #general +k hunter2`<br>`MODE #general -k` |
| `o` | Give or take operator privilege | `MODE #general +o bob`<br>`MODE #general -o bob` |
| `l` | User limit, caps how many can be in the channel | `MODE #general +l 10`<br>`MODE #general -l` |

Several letters go in one command, and the parameters follow in the same order
as the letters that need them: `MODE #general +itk hunter2` turns on invite
only and the topic lock and sets the key in one go. Only `+k`, `+l` and both
signs of `o` take a parameter.

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

update: Additionally after finishing the project we have made clanker tools try to
find niche security bugs and other kind of bugs and some were found so that's great.
