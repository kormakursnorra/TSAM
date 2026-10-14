#include "tsamserver.h"

#include "message_protocol.h"
#include <algorithm>
#include <cerrno>
#include <charconv>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <variant>


int open_socket( int portnum )
{
    struct sockaddr_in sk_addr;   // address settings for bind()
    int sock;                     // socket opened for this port
    int set = 1;                  // for setsockopt

#ifdef __APPLE__     
    if( ( sock = socket(AF_INET, SOCK_STREAM, 0) ) < 0 )
    {
        perror( "Failed to open socket" );
        return -1;
    }
#else
    if( ( sock = socket( AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0 ) ) < 0 )
    {
        perror( "Failed to open socket" );
        return -1;
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
    if( setsockopt( sock, SOL_SOCKET, SOCK_NONBLOCK | SO_NOSIGPIPE, &set, sizeof( set ) ) < 0 )
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
       return -1;
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


int sendMessage( int clientSock, const std::string &replyMsg )
{
    const char *payload = replyMsg.c_str();
    size_t remaining    = replyMsg.length();

    while( remaining > 0 )
    {
        int bytesSent = send( clientSock, payload, remaining, MSG_DONTWAIT | MSG_NOSIGNAL );
        
        if( bytesSent < 0 )
        {
            if( errno == EAGAIN || errno == EWOULDBLOCK )
            {
                continue;
            }
            std::cerr << "Error occured when sending" << std::endl; 
            return -1;
        }

        if( bytesSent == 0 )        // client is gone
        {
            std::cerr << "No response from client" << std::endl; 
            return -1;
        }

        payload   += bytesSent;
        remaining -= bytesSent;
    }

    return 0;
}


std::string handleCommand( const protocol::Payload &payload, Mailboxes &mailBoxes )
{
    if( auto *command = std::get_if< protocol::SendMsg >( &payload ) )
    {
        
        auto &clientBox = mailBoxes[ command->recipient ];
        clientBox.push_back( command->sender + "," + command->contents );
        return "Message sent";
    }
    
    if( auto *command = std::get_if< protocol::GetMsg >( &payload ) )
    {
        auto it = mailBoxes.find( command->username );
        if( it == mailBoxes.end() )
        {
            return "Couldn't find user" + command->username;
        }
        
        auto &clientBox = it->second;
        std::string msg = std::move( clientBox.front() ); // Get the users oldest message
        clientBox.pop_front();                            // Take it off the deque after retrieving
        
        if( clientBox.empty() )
        {
            mailBoxes.erase( it );
        }
        
        return msg;

    }

    if( std::holds_alternative< protocol::StatusReq >( payload ) )
    {
        if( mailBoxes.empty() )
        {
            return "EMPTY";
        }

        std::string response;
        for( const auto &[ username, box ] : mailBoxes )
        {
            if( !response.empty() )
            {
                response += ',';
            }
            response += username + ':' + std::to_string( box.size() );
        }
        
        if( response.size() == protocol::MAX_FRAME )
        {
            return "Size of reply exceeds allowed limit";
        }
        return response;
    }

    return "UNKOWN";
}


int acceptNewClient( int listenSock, ClientLookup &clients, PolledFds &fds )
{
    int clientSock;
    struct sockaddr_in client;
    socklen_t clientLen = sizeof( client );
    
    clientSock = accept( listenSock, ( struct sockaddr * )&client, &clientLen );

    if( clientSock < 0 )
    {
        std::cerr << "Failed to accept new client" << std::endl;
        return -1;
    }

    // Add new client to the list of fds being polled
    fds.push_back( { clientSock, POLLIN, 0 } );

    // Create a new client to store information. Re-use the record for this socket if we have seen it
    // before, so we don't leak a Client every time someone disconnects and reconnects.
    clients.insert_or_assign( clientSock, Client( clientSock ) );
    std::cout << "Client connected: " << clientSock << " from " 
            << inet_ntoa( client.sin_addr ) << ":"  << ntohs( client.sin_port ) << std::endl;
    return 0;
    
}


int receiveFromClient( int sock, char* buffer, Client &client, Mailboxes &mailBoxes )
{
    ssize_t bytesReceieved = recv(sock, buffer, protocol::MAX_FRAME, MSG_DONTWAIT);
    if( bytesReceieved < 0 )
    {
        if( errno == EAGAIN || errno == EINTR )
        {
            std::cerr << "recv interrupted by a signal or client unavailable" << std::endl;
            return 0;
        }
    }
    
    if( bytesReceieved == 0 )
    {
        std::cerr << "Connection closed for sock: " << sock << std::endl;
        return 1;    
    }
    
    client.inbox.append( buffer, bytesReceieved );
    
    while( true ) 
    {
        auto newFrame = protocol::parseMessage( client.inbox );
        
        if( newFrame.status == protocol::FrameStatus::Incomplete )
        {
            break;
        }
        
        if ( newFrame.status == protocol::FrameStatus::Malformed )
        {
            size_t skip = protocol::findNextFrameStart( client.inbox );
            std::cerr << "Malformed input, discarding " << skip << " bytes\n";
            client.inbox.erase(0, skip);
            continue;
        }

        // We need to check what kind of command is being sent.
        std::string replyMessage;
        auto payload = protocol::resolveCommand( newFrame.body ); 
        
        if( payload )
        {
            replyMessage = handleCommand( *payload, mailBoxes );
        }
        else
        {
            replyMessage = "ERROR: Unknown command";
        }
        
        replyMessage = protocol::createFrame( replyMessage );
        client.inbox.erase( 0, newFrame.frameLen );
        if( sendMessage( client.sock, replyMessage ) < 0 )
        {
            return 1;
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

    auto result = std::from_chars(
                argv[1], argv[1] + strlen( argv[1] ), listenPort );

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

    PolledFds fds;     

    if( listen(listenSock, BACKLOG) < 0 )
    {
        std::cerr << "Listen failed on port: " << argv[1] << std::endl;
        exit( 1 );
    }
    else 
    {
        // Add listen socket to the list of fds being polled.
        fds.push_back( { listenSock, POLLIN, 0 } );
    }

    ClientLookup clients;
    Mailboxes mailBoxes;

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
        PolledFds readyFds = fds;

        // First, accept any new connections to the server on the listening socket
        if( readyFds[0].revents & POLLIN )
        {
            if( acceptNewClient( listenSock, clients, fds ) < 0 )
            {
                continue;
            }

            // Decrement the number of sockets waiting to be dealt with
            numFds--;
        }
        
        char buffer[ protocol::MAX_FRAME ];
        
        // Now check for commands from clients
        memset( buffer, 0, sizeof( buffer ) );  

        for( size_t i = 1; i < readyFds.size(); ++i )
        {
            short event = readyFds[i].revents;   
            if( !event )
            {
                continue;    
            }

            int sock = readyFds[i].fd;
            Client &client = clients.at( sock );

            bool disconnect = false;

            if( event & POLLIN )
            {
                int status = receiveFromClient( sock, buffer, client, mailBoxes );
                disconnect = status != 0;
            }

            // Check if socket has a pending error, connection is gone or a fd isn’t open.
            if( event & ( POLLERR | POLLHUP | POLLNVAL ) )
            {
                disconnect = true;
            }

            if( disconnect )
            {
                closeClient( sock, &fds );
                clients.erase( sock );
            }      
        }

    }

    close( listenSock );
    return 0;
}