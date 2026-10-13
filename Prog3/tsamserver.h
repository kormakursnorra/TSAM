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
#include <deque>
#include <list>
#include <map>

// fix SOCK_NONBLOCK for OSX
#ifndef SOCK_NONBLOCK
    #include <fcntl.h>
    #define SOCK_NONBLOCK O_NONBLOCK
#endif

#define BACKLOG  5  

struct Client
{
public:
    int sock;          // socket of client connection
    std::string name;  // Limit length of name of client's user


    Client( int socket ) : sock( socket ){} 

    ~Client(){}       // Virtual destructor defined for base class
};

// Receipients and their messages
using Mailboxes = std::map< std::string, std::deque< std::string > >;

// Lookup table for per Client information
using ClientLookup = std::map< int, Client >;

using PolledFds = std::vector< struct pollfd >;

/* 
Opens a new socket for a specified port. Returns the socket or -1 on error.  
*/ 
int open_socket( int portnum );

/*
Closes a connection between a specified client and server
*/
void closeClient( int clientSocket, PolledFds *fds );

int sendMessage( int clientSock, const std::string &replyMsg );

std::string handleCommand( const protocol::Payload &payload, Mailboxes &boxes  );

/*
Creates a new instance of Client and adds it to the list
*/
int acceptNewClient( int listenSock, ClientLookup &clients, PolledFds &fds ); 

int receiveFromClient( int sock, char* buffer, Client &client, Mailboxes &boxes );


// int onFileDescReady( int listenSock, int clientSock, int &numFds, std::map< int, Client > &clients, PolledFds &fds )
