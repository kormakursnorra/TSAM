#pragma once

#include "message_protocol.h"

#include <netinet/in.h>
#include <map>

struct Client
{
    int sock;                                     // socket of client connection
    std::string name;                             // Limit length of name of client's user
    std::vector< struct proto::Frame* > messages; // The clients messages

    Client( int socket ) : sock( socket ){} 

    ~Client(){}       // Virtual destructor defined for base class
};

class ClientHandler
{
public:
    int numFds;
    std::vector< struct pollfd > fds;
    std::map< int, struct Client* > clients; // Lookup table for per Client information (sock : Client)


    ClientHandler();
    
    int closeClient( int clientSocket, std::vector< struct pollfd > *fds );

    int acceptNewClient( int listenSock );
    
    ~ClientHandler();
};