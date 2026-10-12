#pragma once

#include "message_protocol.h"

#include <netinet/tcp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <string>
#include <vector>
#include <thread>
#include <poll.h>
#include <list>
#include <map>

#define BACKLOG  5  

struct Client
{
public:
    int sock;                                     // socket of client connection
    std::string name;                             // Limit length of name of client's user
    std::vector< struct protocol::Frame* > messages; // The clients messages

    Client( int socket ) : sock( socket ){} 

    ~Client(){}       // Virtual destructor defined for base class
};

/* 
Opens a new socket for a specified port. 
Returns -1 if at any point it fails to do so.  
*/ 
int open_socket( int portnum );

void closeClient( int clientSocket, std::vector< struct pollfd > *fds );

bool sendMessage( int sock, const std::string &msg );

int acceptNewClient( int listenSock, int clientSock, int &numFds, std::vector< struct pollfd > &fds ); 

int receiveFromClient( std::map< int, Client& > &clients, 
                    std::list< Client* > &disconnectedClients,
                    std::vector< struct pollfd > &fds, 
                    std::vector< struct pollfd > &readyFds )


