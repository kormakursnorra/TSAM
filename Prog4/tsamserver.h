#pragma once

#include "message_protocol.h"
#include "protocol_types.h"
#include "connection.h"

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
// TODO: Define caps and timeouts


/* 
Opens a new socket for a specified port. Returns the socket or -1 on error.  
*/ 
int open_socket( int portnum );

/*
Closes a connection between a specified client and server
*/
void closeClient( int clientSocket, connection::PolledFds *fds );

/*
 * Sends a reply message to a specified client. 
 */
int sendMessage( int clientSock, const std::string_view &replyMsg );

/*
 * Unpacks the Payload variant (SendMsg, GetMsg, StatusReq) and takes the appropriate action:
 * If SendMsg   -> Add the given message to the recipients "mailbox".
 * If GetMsg    -> Find the user and return their oldest message.
 * If StatusReq -> Return the number of messages each user has. 
 *
 * Returns a Log containing the reply text (Log::logMsg).
 */
struct logger::Log handleCommand( const protocol::Payload &payload, protocol::Mailboxes &boxes  );

/*
 * Accepts a connection that poll() already reported, adding it to the list of clients and 
 * their sockets (ClientLookup and PolledFds). Returns 0 on success and -1 on error.
*/
int acceptNewClient( int listenSock, connection::ConnectionLookup &clients, connection::PolledFds &fds ); 

/*
 * On reception it processes every complete frame in the buffer (parsing and unpacking), 
 * before sending a reply message. Returns non-zero when client disconnects
 */
int receiveFromClient( int sock, char* buffer, connection::Connection &client, protocol::Mailboxes &boxes );
