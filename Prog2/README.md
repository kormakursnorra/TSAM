# Network Puzzle Solver

A UDP network puzzle solver written in C++. Given a server IP address and four
candidate UDP ports, it identifies and solves the S.E.C.R.E.T., Guardian, Evil
Port, and D.R.A.G.O.N. challenges before performing the final port knock.

## Requirements

- A Linux machine with the POSIX socket API
- `g++` with C++11 support (or `clang++`)
- Root privileges, because the Evil Port challenge uses a raw IPv4 socket
- Access to the course puzzle server

No external libraries are needed.

## Building

### Using Make

Run the following from this directory:

```sh
make
```

This compiles the program and produces an executable named `puzzlesolver`.

To clean up compiled files:

```sh
make clean
```

### Compiling Directly

Alternatively, compile directly with:

```sh
g++ -Wall -Wextra -std=c++11 -o puzzlesolver puzzlesolver.cpp
```

## Running

```sh
sudo ./puzzlesolver <IP address> <port 1> <port 2> <port 3> <port 4>
```

The IP address must be given in IPv4 form. The four ports should be the ports
provided by the puzzle server; their order does not matter because the program
identifies each challenge from its response.

### Example

```sh
sudo ./puzzlesolver <TSAM IP address> 4001 4002 4003 4004
```

## Solving Process

The program solves the challenges in the following order:

1. Scans the four ports and identifies each puzzle from its greeting.
2. Solves the S.E.C.R.E.T. port to obtain the group signature and a hidden port.
3. Solves the Guardian port by constructing a response with an IPv6 and UDP header.
4. Solves the Evil Port by sending a manually constructed IPv4 packet with the evil bit set.
5. Sends the two hidden ports to D.R.A.G.O.N. and receives the port-knock sequence.
6. Knocks on each port using the group signature and Guardian secret spell.

The program prints the responses and progress for each stage. It exits with
status 0 after the final port knock succeeds and status 1 if an error occurs.
