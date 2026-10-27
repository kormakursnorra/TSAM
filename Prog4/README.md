# Peer-to-Peer Server Network

A simple TCP mail server (`tsamserver`) written in C++17. All networking uses the standard POSIX socket API; there
are no third-party dependencies.

## Requirements

* Linux (or macOS) with a C++17 compiler (`g++` 7 or newer, or `clang++`)
* GNU Make
* The Makefile builds with AddressSanitizer and UBSan (`-fsanitize=address,undefined`), so the
  compiler's sanitizer runtime (`libasan`, `libubsan`) must be installed. These ship with GCC on
  most distributions.

## Operating System

Developed and tested on: **Ubuntu 24.04 LTS (x86_64)**

## Building

### Using Make

From the project directory run:

```sh
make
```

This builds `tsamserver`. Object files are placed in `obj/`.
To remove all build output:

```sh
make clean
```

### Compiling Directly

```sh
g++ -std=c++17 -Wall -Wextra -o tsamserver tsamserver.cpp message_protocol.cpp
```

## Running

### Server

```sh
./tsamserver <port> [logDate]
```

* `<port>`: TCP port to listen on (required).
* `[logDate]`: optional. If any second argument is given, the current date and time is meant to
  be added to the log file name. Without it the server logs to `server.log`.

The server runs autonomously and takes no input from the terminal. It listens on all interfaces
(`INADDR_ANY`). Every log entry is written to the log file (opened in append mode) and also
echoed to the terminal: errors to stderr, info and warnings to stdout. Stop the server with
`Ctrl+C`.

### Client

```sh
./tsamclient <IP> <port>
```

* `<IP>`: IPv4 address of the server, e.g. `127.0.0.1`.
* `<port>`: port the server is listening on (1 to 65535).

After connecting, type one command per line and press Enter. The client frames the command,
sends it, waits for the server's reply and prints both with a timestamp. Empty lines are
ignored and commands longer than 5000 bytes are rejected locally. End the session with `Ctrl+D`
(end of input) or `Ctrl+C`.

### Example

Terminal 1:

```sh
./tsamserver 4044
```

Terminal 2:

```text
$ ./tsamclient 127.0.0.1 4044
Connected to server!

STATUSREQ
[2026-10-15 21:13:46] Sending: STATUSREQ
[2026-10-15 21:13:46] Server responded: EMPTY

SENDMSG,alice,bob,Hello Bob, how are you?
[2026-10-15 21:13:52] Sending: SENDMSG,alice,bob,Hello Bob, how are you?
[2026-10-15 21:13:52] Server responded: Message sent

STATUSREQ
[2026-10-15 21:13:55] Sending: STATUSREQ
[2026-10-15 21:13:55] Server responded: bob:1

GETMSG,bob
[2026-10-15 21:13:58] Sending: GETMSG,bob
[2026-10-15 21:13:58] Server responded: alice,Hello Bob, how are you?

GETMSG,bob
[2026-10-15 21:14:01] Sending: GETMSG,bob
[2026-10-15 21:14:01] Server responded: Couldn't find user: bob
```

## Protocol

Every command and every reply is sent as one frame:

```text
<SOH><length><STX><command><ETX>
```

| Field       | Size            | Value                                                                                  |
| ----------- | --------------- | -------------------------------------------------------------------------------------- |
| `<SOH>`     | 1 byte          | `0x01`                                                                                 |
| `<length>`  | 2 bytes         | Unsigned 16-bit, network byte order. Total frame length including the 5 framing bytes. |
| `<STX>`     | 1 byte          | `0x02`                                                                                 |
| `<command>` | 0 to 5000 bytes | The command text                                                                       |
| `<ETX>`     | 1 byte          | `0x03`                                                                                 |

So a valid frame is between 5 and 5005 bytes long. Both the server and the client keep a
per-connection receive buffer, because TCP may deliver a frame in several pieces or several
frames in one piece. A command is only processed once its whole frame has arrived.

## Commands

Fields are separated by commas. Command names are case sensitive.

| Command | Server behaviour      | Reply                                                                                                                                                           |
| ------- | --------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `SENDMSG,<from>,<to>,<message>` | Appends the message to the end of `<to>`'s mailbox. Everything after the third comma is the message, so the message itself may contain commas. | `Message sent` |
| `GETMSG,<username>`             | Removes and returns the **oldest** stored message for `<username>` (first in, first out). | `<from>,<message>`, or `Couldn't find user: <username>` if the user has no messages |
| `STATUSREQ`                     | Lists every user that has at least one stored message. | `<user1>:<count1>,<user2>:<count2>,...` with no spaces, or exactly `EMPTY` if no messages are stored. Users are listed in alphabetical order. |

No additional commands are implemented.

### Invalid commands

A well-framed command that is not valid is not executed. The server logs a warning or error and
replies with a short description:

| Situation | Reply |
| --------- | ----- |
| Empty command body | `Command body is empty` |
| `SENDMSG` with empty or missing `<from>` or `<to>` | `Missing sender and/or recipient` |
| `SENDMSG` with empty or missing `<message>` | `Missing message contents` |
| `GETMSG` with empty or missing `<username>` | `Missing username` |
| `STATUSREQ` followed by extra fields | `STATUSREQ must have empty body` |
| Any other command name | `Unknown command` |

Messages are stored in memory only and are lost when the server stops.

## Server Behaviour

* **Concurrency.** The server is single-threaded and uses `poll()` to wait on the listening
  socket and all client sockets at once. It blocks in `poll()` with no timeout, so it uses no
  CPU while idle. Several clients can be connected at the same time and each can send any number
  of commands over one connection.
* **New connections.** Each accepted client is added to the poll set and gets its own receive
  buffer. The client's address and port are logged.
* **Malformed input.** If the bytes in a client's buffer do not form a valid frame (the first
  byte is not `SOH`, the fourth byte is not `STX`, the length field is outside 5 to 5005, or the
  byte at the declared end is not `ETX`), the server logs it, discards bytes up to the next
  `SOH` (or the whole buffer if there is none) and tries again. No reply is sent for discarded
  bytes and the connection stays open. Frames longer than 5005 bytes are treated as malformed.
* **Partial frames.** If only part of a frame has arrived, the server keeps the bytes and waits
  for more data.
* **Disconnects.** When a client closes the connection, `recv()` fails, or `poll()` reports
  `POLLERR`, `POLLHUP` or `POLLNVAL`, the socket is closed and removed from the poll set and the
  client's record is deleted. Stored mailboxes are not tied to connections, so messages survive
  clients disconnecting and reconnecting.
* **Slow or stuck clients.** Each client socket has a 2 second send timeout (`SO_SNDTIMEO`). If a
  reply cannot be sent the client is disconnected. `SIGPIPE` is suppressed (`MSG_NOSIGNAL` on
  Linux, `SO_NOSIGPIPE` on macOS) so a client vanishing mid-send cannot kill the server.
* **Oversized STATUSREQ reply.** If the status list would exceed 5000 bytes, the server replies
  with `Size of reply is equal to or exceeds maximum` instead.

## Logging

The server writes a timestamped entry for every received command, every reply, every connect and
disconnect, and every framing or command error, for example:

```text
[ 2026-10-15 21:13:46 ] [ INFO ] Client connected: 4 from 127.0.0.1:51234
[ 2026-10-15 21:13:52 ] [ INFO ] Received from sock 4: SENDMSG,alice,bob,Hello Bob, how are you?
[ 2026-10-15 21:13:52 ] [ INFO ] Reply to sock 4: Message sent
[ 2026-10-15 21:14:05 ] [ INFO ] Connection closed for clientSock: 4
```

## Client Behaviour

* Reads commands from stdin and blocks in `recv()` until the reply frame has arrived, so it does
  not busy-wait.
* Uses the same framing code as the server, so it also buffers partial replies and skips
  malformed bytes from the server (printing a notice to stderr).
* Prints every command sent and every reply received with a `YYYY-MM-DD HH:MM:SS` timestamp.
* Exits if the server closes the connection or a send or receive error occurs.

## Files

| File | Purpose |
| ---- | ------- |
| `tsamserver.cpp`, `tsamserver.h` | Server: socket setup, poll loop, command handling |
| `tsamclient.cpp`, `tsamclient.h` | Client |
| `message_protocol.cpp`, `message_protocol.h` | Shared framing and command parsing |
| `logger.cpp`, `logger.h` | Timestamped logging |
| `Makefile` | Builds both executables |