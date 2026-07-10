# ft_irc - Architecture notes

C++98, `-Wall -Wextra -Werror -std=c++98`. External libraries are forbidden.
Run as `./ircserv <port> <password>`.

---

## 0. Things to avoid 

- More than one `poll()` call in the entire codebase. 
- Calling `recv`/`send` on a fd without checking `poll` first.
  (Super important, I know someone who failed their project because of this)
- `fcntl` used for anything other than `fcntl(fd, F_SETFL, O_NONBLOCK)`.
- Forking or threading.
- The server crashing or exiting because of a bad client. `exit()` is only okay
  at startup if args are wrong or the socket fails to bind.
- SIGPIPE not ignored. If you `send()` to a client that already closed the
  connection, the default handler kills your process. One line at startup fixes it:
  `signal(SIGPIPE, SIG_IGN);`

> **Why the poll rule matters:** someone got a 0 on their defense because they
> called `send()` directly inside a command handler without going through poll/POLLOUT
> first. The evaluator cited the subject text and failed their project D:. Every send goes
> through the send buffer, the loop handles the actual write.

## 1. Syscalls we use

No need for most of what the subject lists:

`socket, setsockopt(SO_REUSEADDR), bind, listen, accept, fcntl(O_NONBLOCK),
poll, recv, send, close, signal(SIGPIPE/SIGINT), htons, htonl`

`sockaddr_in` with `htonl(INADDR_ANY)` is enough to bind on all interfaces.
No need for getaddrinfo or gethostbyname.

C++ stdlib is fine (`std::string`, `std::map`, `std::vector`, etc.), the
subject only restricts network/system calls.

---

## 2. The event loop

```
setup: socket -> setsockopt(REUSEADDR) -> bind -> listen -> fcntl(NONBLOCK)
       signal(SIGPIPE, SIG_IGN)
       signal(SIGINT, &set_stop_flag)

loop while (!stop_flag):
    build pollfd list:
        listen fd       -> POLLIN
        each client fd  -> POLLIN | (POLLOUT if send_buf not empty)
    poll(...)           // the only poll call

    for each ready fd:
        listen  + POLLIN            -> accept, set O_NONBLOCK, add Client
        client  + POLLHUP/POLLERR   -> disconnect
        client  + POLLIN            -> recv once, append to recv_buf, process complete lines
        client  + POLLOUT           -> send what the kernel takes, trim send_buf

shutdown: close everything, STL cleans up the rest
```

The key thing to get right from the start: **command handlers never call send()
or recv() directly.** They write into `client.send_buf`, that's it. The loop above
is the only place that touches the network. This is what keeps the eval-killer bug
from creeping in.

The pollfd list gets rebuilt every iteration from scratch. At IRC scale this is
nothing, and it means you never have to worry about keeping it in sync with the
client map.

### Reading lines

Split on `\n`, strip the `\r` if there is one. Both `\r\n` (what irssi and the
RFC use) and bare `\n` (what plain `nc` sends) have to work. The eval will test
with plain `nc`, if you only handle `\r\n` you'll be silent for the entire defense.

### Partial reads and writes

One `recv` call might give you half a line, or four lines. Append everything to
`recv_buf`, extract only the complete lines, leave the rest.

Same idea for writes: `send` might not take everything you give it. Only remove
what actually got sent from `send_buf`, the rest stays and goes out next time
POLLOUT fires. This is also how the `^Z` test works, a paused client just
accumulates in its send buffer and catches up when it resumes.

---

## 3. Message format

IRC messages look like: `[:prefix] COMMAND [params] [:trailing]`

Clients never send the prefix part. Parse into:

```cpp
struct Message {
    std::string              command;  // uppercased
    std::vector<std::string> params;   // everything after " :" goes in the last slot
};
```

One `parse()` function, ~20 lines. Split on spaces, and if a token starts with `:`
it swallows the rest of the line as one param.

---

## 4. Classes

```cpp
class Client {
    int fd;
    std::string nick, user, host;
    bool has_pass, registered;
    std::string recv_buf, send_buf;
    std::set<std::string> channels;
};

class Channel {
    std::string name, topic, key;
    std::set<int> members;
    std::set<int> operators;
    bool invite_only, topic_locked;
    size_t user_limit;   // 0 = no limit
    std::set<int> invited;
};

class Server {
    int listen_fd;
    std::string password;
    std::map<int, Client> clients;
    std::map<std::string, Channel> channels;
};
```

Objects live by value inside the maps, so there's nothing to manually free.
`clients.erase(fd)` handles cleanup automatically.

`disconnect(fd)` needs to: remove the client from all its channels (and delete
any channel that goes empty), `close(fd)`, then erase from the clients map.
Keep this in one function called from one place.

---

## 5. Command dispatch

```cpp
typedef void (Server::*Handler)(Client&, const Message&);
std::map<std::string, Handler> handlers;
```

Unknown command: send `421`, don't disconnect. irssi sends `CAP LS` when it
connects, ignore it, no reply needed.

Clients that haven't finished registering can only use PASS, NICK, USER, QUIT.
Everything else gets a `451`.

PING/PONG is mandatory, not a bonus. irssi will drop the connection if the server
doesn't reply to its pings. Handle it early.

---

## 6. Registration and commands

Registration order: `PASS` -> `NICK` -> `USER`. Once all three are good, mark
the client registered and send `001`. Wrong password -> `464`. Nick taken -> `433`.

### Numeric replies we need

```
001 welcome                 431/432/433 nick errors
461 not enough params       462 already registered      464 bad password
451 not registered          401 no such nick            403 no such channel
421 unknown command         442 not on channel          482 not an operator
441 user not in channel     324 channel modes
471 channel full  473 invite only  475 wrong key        331/332 topic  341 invited
353/366 names list
```

### Message format for commands

Most commands echo back to clients as `:nick!user@host COMMAND args`, not a numeric.

PRIVMSG goes to either a nick or a #channel. Bad nick -> `401`, bad channel -> `403`.

When someone changes their nick, broadcast `:oldnick!user@host NICK newnick` to
everyone in their channels, or other clients will have the wrong name.

`MODE #chan` with no arguments is a mode query, reply with `324`. irssi sends this
on every join.

---

## 7. File layout

```
ft_irc/
├── Makefile
├── README.md
├── ARCHITECTURE.md
├── includes/
│   └── Server.hpp  Client.hpp  Channel.hpp  Message.hpp
└── srcs/
    ├── main.cpp  Server.cpp  Client.cpp  Channel.cpp  Message.cpp
    └── commands/    (phase 2 files go here)
```

New headers in `includes/`, new sources in `srcs/`, add to `SRCS` in Makefile.

### What to build together first

Get the poll loop working end to end before splitting anything: socket setup,
accept, recv into buffers, line extraction, send queue draining. Once that's good
and tested with `nc`, agree on the final header shapes and move to phase 2.

### Phase 2 split

| Who | Files |
|-----|-------|
| A | `cmd_register.cpp` (PASS/NICK/USER/PING), `cmd_message.cpp` (PRIVMSG/NOTICE/QUIT), `cmd_join.cpp` (JOIN/PART) |
| B | `cmd_mode.cpp` (MODE i,t,k,o,l + query), `cmd_oper.cpp` (KICK/INVITE/TOPIC) |

---

## 8. Bonuses

Only evaluated if the mandatory part is perfect.
(Probably will pass on this unless super easy because gotta go fast and finish the common core)

- **File transfer (DCC)**: the clients handle this themselves over a direct
  connection. The server just has to pass the PRIVMSG through without mangling it
  (the CTCP `\x01` bytes and everything). If PRIVMSG is byte-exact, DCC works.
- **Bot**: a separate binary that connects as a normal client. No server changes
  needed beyond what's already mandatory (PING/PONG, NOTICE).

---

## 9. Startup

```
argc != 3                -> usage, exit 1
port has non-digit chars -> error, exit 1
port < 1 || port > 65535 -> error, exit 1
password empty           -> error, exit 1
```

---

## Reference client

**irssi**. Test with it from the start.

`bircd.tar.gz` is a C reference implementation using `select()`, useful to read
for understanding the loop structure, but don't submit any of it.

---

Once the core server is good (poll loop, accept, recv/send buffering, line
extraction all working and tested with `nc`), the IRC commands can start being implemented. :)
