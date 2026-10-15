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

#define BACKLOG  5  

/*
 * Wrapper for a connected clients data.  
 */
struct Client
{
public:
    int sock;           // socket of client connection
    std::string name;   // Limit length of name of client's user
    std::string inbox;  // bytes received but not yet parsed

    Client( int socket ) : sock( socket ){} 

    ~Client(){}     
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

/*
 * Sends a reply message to a specified client. 
 */
int sendMessage( int clientSock, const std::string_view &replyMsg );

/*
 * Unpacks the Payload variant (SendMsg, GetMsg, StatusReq) and takes the appropriate action:
 * If SendMsg   -> Add the given message to the recipients "mailbox".
 * If GetMsg    -> Find the user and return their oldest message.
 * If StatusReq -> Return the number of messages each user has. 
 */
struct logger::Log handleCommand( const protocol::Payload &payload, Mailboxes &boxes  );

/*
 * Creates a new client socket and waits for connection to happen. If a client connects, add 
 * it to the list of other clients (ClientLookup) and their sockets (PolledFds). 
*/
int acceptNewClient( int listenSock, ClientLookup &clients, PolledFds &fds ); 

/*
 * Waits to receive a message from a specified client. On reception, verifies and unpack the 
 * given message before sending a reply message. 
 */
int receiveFromClient( int sock, char* buffer, Client &client, Mailboxes &boxes );
