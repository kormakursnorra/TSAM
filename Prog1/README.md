# UDP Port Scanner

A simple UDP port scanner written in C++. Given an IP address and a range of ports,
it sends a UDP datagram to every port in the range and reports a port as **open** if
a reply comes back before the timeout expires.

## Requirements

- A Linux/macOS machine (uses the POSIX socket API: `<sys/socket.h>`, `<netinet/in.h>`, `<arpa/inet.h>`)
- `g++` with C++11 support (or `clang++`)
- No external libraries are needed

## Building

Compile directly with:

```sh
g++ -Wall -Wextra -std=c++11 -o scanner scanner.cpp
```

This produces an executable named `scanner` in the current directory.

## Running

```sh
./scanner <IP address> <low port> <high port>
```

All three arguments are required. The IP address must be given in IPv4 form.

### Example

Scanning UDP ports 4000–4100 on the TSAM server:

```sh
./scanner <TSAM IP address> 4000 4100
```

Sample output:

```
Port 4021 is open
Port 4045 is open
```

Ports that do not respond are simply not printed. The program exits with status 0
on success and 1 if the arguments are invalid, the socket cannot be created, the
receive timeout cannot be set, or the IP address cannot be parsed.

### Verifying with nmap

The results can be cross-checked against nmap's UDP scan (which needs root
privileges because it inspects ICMP replies):

```sh
sudo nmap -sU -p 4000-4100 <TSAM IP address>
```