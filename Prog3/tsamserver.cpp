#include "tsamserver.h"
#include "message_protocol.h"
#include <algorithm>
#include <cerrno>
#include <charconv>
#include <iostream>
#include <list>


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

int acceptNewClient( int listenSock, int clientSock, std::map< int, Client& > &clients, std::vector< struct pollfd > &fds )
{
    struct sockaddr_in client;
    socklen_t clientLen;
    
    clientSock = accept( listenSock, ( struct sockaddr * )&client, &clientLen );

    if( clientSock < 0 )
    {
        std::cerr << "Failed to accept new client" << std::endl;
        return ( -1 );
    }

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

    std::cout << "Client connected on server: " << clientSock << std::endl;
    return 0;
    
}

int receiveFromClient( int sock, char* buffer, std::string &pending, Client &client )
{
    size_t bytesReceieved = recv(sock, buffer, sizeof(buffer), MSG_DONTWAIT);
    if( bytesReceieved == 0)
    {
        std::cerr << "Connection closed for sock: " << sock << std::endl;
        return ( 1 );    
    }
    
    struct protocol::Frame newFrame;

    pending.append( buffer, bytesReceieved );
    
    while( true ) 
    {
        newFrame = protocol::parseMessage( pending );
        
        if( newFrame.status == protocol::ParseStatus::Ok )
        {
            // We need to check what kind of command is being sent.
            auto command = buf.substr( protocol::STX, newFrame.frameLen - 1 );
            protocol::Payload payload; 

            if( resolveCommand( command, payload ) > 0 )
            {
                std::cerr << "Unable to resolve command" << std::endl;
                return { ParseStatus::Malformed };
            }
            
            client.messages.emplace(client.messages.size(), &newFrame.payload );
            pending.erase(0, newFrame.frameLen);
        }
        else if ( newFrame.status == protocol::ParseStatus::Incomplete )
        {
            break;
        }
        else 
        {
            std::size_t skip = protocol::findNextFrameStart( pending, newFrame );
            std::cerr << "Malformed input, discarding " << skip << " bytes\n";
            pending.erase(0, skip);
        }
    }

    return 0;     
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

    auto result = std::from_chars(
                argv[1], argv[1] + sizeof( &argv[1] ) - 1, listenPort );

    if( result.ec != std::errc() )
    {
        std::cerr << "Bad argument error <ip port>" << std::endl;
        exit( 1 );
    }

    // Open socket to listen to, if it fails exit with status 1
    if ( ( listenSock = open_socket( listenPort ) ) < 0 )
    {
        exit( 1 );
    }

    std::vector< struct pollfd > fds;     

    if( listen(listenSock, BACKLOG) < 0 )
    {
        std::cerr << "Listen failed on port: " << argv[1] << std::endl;
        exit( 0 );
    }
    else 
    {
        // Add listen socket to the list of fds being polled.
        fds.push_back( { listenSock, POLLIN, 0 } );
    }

    std::map< int, Client& > clients; // Lookup table for per Client information
    
    while( true )
    {
        // Wait (indefinitely) until at least one of our sockets has
        // something to be read() on it.
        int numFds = poll( fds.data(), fds.size(), -1 );

        if( numFds < 0 )
        {
            // if interrupted by singal, continue
            if( errno == EINTR )
            {
                continue;
            }
            
            std::cerr << "Poll failed - Closing down" << std::endl;
            break;
        }

        // Take a snapshot of the current fds/revents before processing
        // them below, since fds itself may be modified while we go
        // (new clients connecting, existing clients disconnecting).
        std::vector< struct pollfd > readyFds = fds;

        // First, accept any new connections to the server on the listening socket
        if( readyFds[0].revents & POLLIN )
        {
            if( acceptNewClient( listenSock, clientSock, clients, fds ) < 0 )
            {

                continue;
            }

            // Decrement the number of sockets waiting to be dealt with
            numFds--;
        }
        
        std::vector< struct Client* > disconnectedClients;
        char buffer[ protocol::MAX_FRAME ];
        std::string pendingMessage;
        
        // Now check for commands from clients
        while( numFds-- > 0 )
        {
            memset( buffer, 0, sizeof( buffer ) );  

            for( size_t i = 1; i < readyFds.size(); i++ )
            {
                if( readyFds[i].revents & POLLIN)
                {
                    int sock = readyFds[i].fd;
                    Client &client = clients.at( sock );
                    
                    receiveFromClient( sock, buffer, pendingMessage, client );

                    disconnectedClients.push_back( &client );
                    closeClient(sock, &fds);

                }
            }

            // Remove client from the clients list
            for( auto const &c : disconnectedClients )
            {
                clients.erase( c->sock );
            }    
        }
    }

    return 0;
}