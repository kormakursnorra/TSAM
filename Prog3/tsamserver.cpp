#include "tsamserver.h"
#include <algorithm>
#include <iostream>
#include <list>

std::map<int, Client*> clients; // Lookup table for per Client information

int open_socket( int portnum )
{
    struct sockaddr_in sk_addr;   // address settings for bind()
    int sock;                     // socket opened for this port
    int set = 1;                  // for setsockopt

#ifdef __APPLE__     
    if( ( sock = socket(AF_INET, SOCK_STREAM, 0) ) < 0 )
    {
        perror( "Failed to open socket" );
        return ( -1 );
    }
#else
    if( ( sock = socket( AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0 ) ) < 0 )
    {
        perror( "Failed to open socket" );
        return ( -1 );
    }
#endif

    // Turn on SO_REUSEADDR to allow socket to be quickly reused after 
    // program exit.

    if(setsockopt( sock, SOL_SOCKET, SO_REUSEADDR, &set, sizeof( set )) < 0 )
    {
       perror( "Failed to set SO_REUSEADDR:" );
    }
    set = 1;
#ifdef __APPLE__     
    if( setsockopt( sock, SOL_SOCKET, SOCK_NONBLOCK, &set, sizeof( set ) ) < 0 )
    {
      perror( "Failed to set SOCK_NOBBLOCK" );
    }
#endif
    memset( &sk_addr, 0, sizeof( sk_addr ) );

    sk_addr.sin_family      = AF_INET;
    sk_addr.sin_addr.s_addr = INADDR_ANY;
    sk_addr.sin_port        = htons( portnum );

    // Bind to socket to listen for connections from clients

    if( bind( sock, ( struct sockaddr * )&sk_addr, sizeof( sk_addr )) < 0)
    {
       perror( "Failed to bind to socket:" );
       return ( -1 );
    }
    else
    {
       return ( sock );
    }

}

void closeClient( int clientSocket, std::vector< struct pollfd > *fds )
{
    printf( "Client closed connection: %d\n", clientSocket );

    close( clientSocket );      

    // Find and remove this socket's entry from the list of fds polled.
    auto fdIt = std::find_if( fds->begin(), fds->end(), 
                    [ clientSocket ]( const struct pollfd &pfd )
                    {
                        return pfd.fd == clientSocket;
                    } );

    if( fdIt != fds->end() )
    {
        fds->erase( fdIt );
    }
}


bool sendMessage( int sock, const std::string &msg )
{
    const char *p    = msg.c_str();
    size_t remaining = msg.length();

    while( remaining > 0 )
    {
        int n = send( sock, p, remaining, MSG_DONTWAIT );

        if( n == 0 )        // client is gone
        {
            return false;
        }

        p         += n;
        remaining -= n;
    }

    return true;
}

int main( int argc, char* argv[] )
{
    // Process command line argument/s
    
    if( argc != 2 )
    {
        perror( "Missing argument <ip port>\n" );
        exit( 1 );   
    }
    
    int listenPort = 0;
    int listenSock = -1;
    int clientSock = -1;

    if( ( listenPort = atoi( argv[1] ) ) == 0 )
    {
        perror( "Bad argument error <ip port>\n" );
        exit( 1 );
    }

    // Open socket to listen to, if it fails exit with status 1
    if ( ( listenSock = open_socket( listenPort ) ) < 0 )
    {
        exit( 1 );
    }

    std::vector<struct pollfd> fds;

    struct sockaddr_in client;
    socklen_t clientLen;
    char buffer[5000];     

    if( listen(listenSock, BACKLOG) < 0 )
    {
        printf( "Listen failed on port %s\n", argv[1] );
        exit( 0 );
    }
    else 
    // Add listen socket to the list of fds being polled.
    {
        fds.push_back( { listenSock, POLLIN, 0 } );
    }

    bool finished = false;
    
    while( !finished )
    {
        memset( buffer, 0, sizeof( buffer ) );

        // Wait (indefinitely) until at least one of our sockets has
        // something to be read() on it.
        int n = poll( fds.data(), fds.size(), -1 );

        if( n < 0 )
        {
            perror( "poll failed - closing down\n" );
            finished = true;
        }
        else
        {
            // Take a snapshot of the current fds/revents before processing
            // them below, since fds itself may be modified while we go
            // (new clients connecting, existing clients disconnecting).
            std::vector< struct pollfd > readyFds = fds;

            // First, accept any new connections to the server on the listening socket
            if( readyFds[0].revents & POLLIN )
            {
                clientSock = accept( listenSock, ( struct sockaddr * )&client,
                                   &clientLen );
                
                printf( "accept***\n" );
                // Add new client to the list of fds being polled
                fds.push_back( { clientSock, POLLIN, 0 } );

                // create a new client to store information.
                // Re-use the record for this socket if we have seen it
                // before, so we don't leak a Client every time someone
                // disconnects and reconnects.
                if(clients.find( clientSock ) == clients.end())
                {
                    clients[ clientSock ] = new Client( clientSock );
                }

               // Decrement the number of sockets waiting to be dealt with
               n--;

               printf("Client connected on server: %d\n", clientSock);
            }
            
            // Now check for commands from clients
            std::list< Client * > disconnectedClients;  
            
            while( n-- > 0 )
            {
                for( size_t i = 1; i < readyFds.size(); i++ )
                {
                    if(readyFds[i].revents & POLLIN)
                    {
                        int sock = readyFds[i].fd;
                        Client *client = clients[sock];

                        // recv() == 0 means client has closed connection
                        if(recv(sock, buffer, sizeof(buffer), MSG_DONTWAIT) == 0)
                        {
                            disconnectedClients.push_back(client);
                            closeClient(sock, &fds);

                        }
                        // We don't check for -1 (nothing received) because poll()
                        // only triggers if there is something on the socket for us.
                        else
                        {
                            std::cout << buffer << std::endl;
                            clientCommand(sock, &fds, buffer);
                        }
                    }
                }
                // Remove client from the clients list
                for( auto const& c : disconnectedClients )
                {
                    clients.erase( c->sock );
                }    
            }
        }
    }

    return 0;
}