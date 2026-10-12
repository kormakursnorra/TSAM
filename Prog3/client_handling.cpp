#include "client_handling.h"

#include <sys/poll.h>
#include <iostream>

int ClientHandler::closeClient( int clientSocket, )


int ClientHandler::acceptNewClient( int listenSock )
{
    int clientSock = -1;
    struct sockaddr_in client{};
    socklen_t clientLen = sizeof( client );

    // Take a snapshot of the current fds/revents before processing
    // them below, since fds itself may be modified while we go
    // (new clients connecting, existing clients disconnecting).
    std::vector< struct pollfd > readyFds = fds;

    // First, accept any new connections to the server on the listening socket
    if( readyFds[0].revents & POLLIN )
    {
        clientSock = accept( listenSock, ( struct sockaddr * )&client,
                            &clientLen );
        
        std::cout << "Accept***" << std::endl;
        
        // Add new client to the list of fds being polled
        fds.push_back( { clientSock, POLLIN, 0 } );

        // create a new client to store information.
        // Re-use the record for this socket if we have seen it
        // before, so we don't leak a Client every time someone
        // disconnects and reconnects.
        if( clients.find( clientSock ) == clients.end() )
        {
            clients.insert_or_assign( clientSock, Client( clientSock ) );
        }

        // Decrement the number of sockets waiting to be dealt with
        numFds--;

        std::cout << "Client connected on server: " << clientSock << std::endl;
    }
}