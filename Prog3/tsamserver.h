#pragma once

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
#include <vector>
#include <poll.h>

#define BACKLOG  5  

class Client
{
  public:
    int sock;         // socket of client connection
    std::string name; // Limit length of name of client's user

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

void clientCommand( int clientSocket, std::vector<struct pollfd> *fds, char *buffer );


