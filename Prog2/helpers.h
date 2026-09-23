#pragma once

#include <asm-generic/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/udp.h>
#include <sys/socket.h>

#include <cstdint>
#include <vector>
#include <string>
#include <map>

#define MAX_RETRIES 5
#define TIMEOUT_MS 10


/*
Parameters for socket initialization. If not specified, it defaults to IPv4 UDP: 
*/
struct SocketOpt
{
    int level;
    int optname;
    const void* optval;
    socklen_t optlen;
};

struct SocketParams
{
    int domain   = AF_INET;
    int type     = SOCK_DGRAM;
    int protocol = 0;
    struct SocketOpt option;
};


struct Signature 
{
    uint8_t groupId;
    uint32_t secretSigil;
};

struct Ports
{
    int evilPort;
    int secretPort;
    int dragonPort;  
    int guardianPort;
};

struct SecretData 
{
    int secretPort;
    int hiddenPort;
    Signature signature;
};

struct GuardianData 
{
    int guardianPort;
    Signature signature;
    std::string secretSpell;
    struct ip6_hdr responseHdr;
    struct udphdr responseUdpHdr;  
};

struct EvilBitData 
{
    int evilPort;
    int HiddenPort;
    struct ip ipv4Hdr;
    struct udphdr udpHdr;
    Signature signature;  
};

/* 
Initializes a new socket with specified parameters and sets a timeout for receive tolerance.
Returns -1 on error and the socket file descriptor on success. 
*/
int initializeSocket( const struct SocketParams params = SocketParams{} );


/* 
Sends a packet to a pre-specified (before function call) destination and writes the response to a buffer.
The function will try MAX_RETRIES (5) times if no response is received. 

On success it returns the number of bytes received, 0 if no response after exhausting retries and -1 if an error occurs while sending or receiving data.
*/
int sendAndReceive( const int sockfd, std::string data, char* buff, size_t buffSize ); 


/* 
Assembles the initial S.E.C.R.E.T. handshake message:
    1. The literal prefix S.E.C.R.E.T.:
    2. The group members usernames
    3. A 32-bit random number appended as four raw bytes in network order.
*/
void constructMessage( std::string &secretMessage, uint32_t &secretNumber, const std::string &userNames );

/*
Maps the port numbers to the expected byte count of their respective greetings. 
A matching key gets the value of the port number that produced it. 

Returns 0 if the byte count matched a port number and the port was recorded and 1 if no key matched the byte count.
*/
int mapToPort( const int bytesReceived, const int port, std::map< int, int >& portMap );


void buildIpv6Buffer( std::vector< uint8_t >& buff, const struct ip6_hdr& iphdr, 
    struct udphdr udphdr, const char* payload, size_t payloadLen );

void buildIpv4Buffer( std::vector< uint8_t >& buff, const struct ip& ipv4hdr );

int checksum( std::vector< uint8_t > buff );