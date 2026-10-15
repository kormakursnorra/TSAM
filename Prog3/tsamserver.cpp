#include "tsamserver.h"
#include "logger.h"

#include "message_protocol.h"
#include <algorithm>
#include <cerrno>
#include <charconv>
#include <iostream>
#include <netinet/in.h>
#include <ostream>
#include <string>
#include <system_error>
#include <variant>


int open_socket( int portnum )
{
    struct sockaddr_in sk_addr;   // address settings for bind()
    int sock;                     // socket opened for this port
    int set = 1;                  // for setsockopt

#ifdef __APPLE__     
    if( ( sock = socket(AF_INET, SOCK_STREAM, 0) ) < 0 )
    {
        logger::logToFile( { logger::LogLevel::ERROR, "Failed to open socket" } );
        return -1;
    }
#else
    if( ( sock = socket( AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0 ) ) < 0 )
    {
        logger::logToFile( { logger::LogLevel::ERROR, "Failed to open socket" } );
        return -1;
    }
#endif

    // Turn on SO_REUSEADDR to allow socket to be quickly reused after 
    // program exit.

    if(setsockopt( sock, SOL_SOCKET, SO_REUSEADDR, &set, sizeof( set )) < 0 )
    {
       logger::logToFile( { logger::LogLevel::ERROR, "Failed to set SO_REUSEADDR" } );
    }
    set = 1;

#ifdef __APPLE__     
    if( setsockopt( sock, SOL_SOCKET, SOCK_NONBLOCK | SO_NOSIGPIPE, &set, sizeof( set ) ) < 0 )
    {
      logger::logToFile( { logger::LogLevel::ERROR, "Failed to set SOCK_NOBBLOCK" } );
    }
#endif
    memset( &sk_addr, 0, sizeof( sk_addr ) );

    sk_addr.sin_family      = AF_INET;
    sk_addr.sin_addr.s_addr = INADDR_ANY;
    sk_addr.sin_port        = htons( portnum );

    // Bind to socket to listen for connections from clients

    if( bind( sock, ( struct sockaddr * )&sk_addr, sizeof( sk_addr )) < 0)
    {
       logger::logToFile( { logger::LogLevel::ERROR, 
                                  "Failed to bind to socket: " + std::to_string( sock ) } );
       return -1;
    }
    else
    {
       logger::logToFile( { logger::LogLevel::INFO, 
                                  "Binded to sock: " + std::to_string( sock ) } ); 
       return ( sock );
    }

}


void closeClient( int clientSocket, std::vector< struct pollfd > *fds )
{
    logger::logToFile( { logger::LogLevel::ERROR, 
                               "Client closed connection: " + std::to_string( clientSocket ) } );

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


int sendMessage( int clientSock, const std::string_view &replyMsg )
{
    std::string frame   = protocol::createFrame( replyMsg );
    const char *payload = frame.c_str();
    size_t remaining    = frame.length();

    while( remaining > 0 )
    {
        int bytesSent = send( clientSock, payload, remaining, MSG_NOSIGNAL );
        
        if( bytesSent < 0 )
        {
            if( errno == EINTR )
            {
                continue;
            }

            logger::logToFile( { logger::LogLevel::ERROR, "Error occured when sending" } );
            return -1;
        }

        if( bytesSent == 0 ) // client is gone
        {
            logger::logToFile( { logger::LogLevel::ERROR, "No response from client" } ); 
            return -1;
        }

        payload   += bytesSent;
        remaining -= bytesSent;
    }

    return 0;
}


struct logger::Log handleCommand( const protocol::Payload &payload, Mailboxes &mailBoxes )
{
    if( auto *command = std::get_if< protocol::SendMsg >( &payload ) )
    {
        
        auto &clientBox = mailBoxes[ command->recipient ];
        clientBox.push_back( command->sender + "," + command->contents );
        return { logger::LogLevel::INFO, "Message sent" };
    }
    
    if( auto *command = std::get_if< protocol::GetMsg >( &payload ) )
    {
        auto it = mailBoxes.find( command->username );
        if( it == mailBoxes.end() )
        { 
            return { logger::LogLevel::WARNING, "Couldn't find user: " + command->username };
        }
        
        auto &clientBox = it->second;
        std::string msg = std::move( clientBox.front() ); // Get the users oldest message
        clientBox.pop_front();                            // Take it off the deque after retrieving
        
        if( clientBox.empty() )
        {
            mailBoxes.erase( it );
        }
        
        return { logger::LogLevel::INFO, msg };

    }

    if( std::holds_alternative< protocol::StatusReq >( payload ) )
    {
        if( mailBoxes.empty() )
        {
            return { logger::LogLevel::WARNING, "EMPTY" };
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
        
        if( response.size() > protocol::MAX_FRAME )
        {
            return { logger::LogLevel::WARNING, "Size of reply is equal to or exceeds MAX_FRAME" };
        }
        return { logger::LogLevel::INFO, response };
    }

    return { logger::LogLevel::ERROR, "UNKNOWN" };
}


int acceptNewClient( int listenSock, ClientLookup &clients, PolledFds &fds )
{
    int clientSock;
    struct sockaddr_in client;
    socklen_t clientLen = sizeof( client );
    
    clientSock = accept( listenSock, ( struct sockaddr * )&client, &clientLen );

    if( clientSock < 0 )
    {
        logger::logToFile( { logger::LogLevel::ERROR, "Failed to accept new client" } ); 
        return -1;
    }

    struct timeval tv = { .tv_sec = 2, .tv_usec = 0 };
    if ( setsockopt(clientSock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0 )
    {
        logger::logToFile( { logger::LogLevel::ERROR, "Error setting socket option to SO_SNDTIMEO" } ); 
        return -1;
    }

    // Add new client to the list of fds being polled
    fds.push_back( { clientSock, POLLIN, 0 } );

    // Create a new client to store information. Re-use the record for this socket if we have seen it
    // before, so we don't leak a Client every time someone disconnects and reconnects.
    clients.insert_or_assign( clientSock, Client( clientSock ) );

    std::string logMsg = "Client connected: " + std::to_string( clientSock ) + " from " 
            + inet_ntoa( client.sin_addr ) + ":"  + std::to_string( ntohs( client.sin_port ) ); 

    logger::logToFile( { logger::LogLevel::INFO, logMsg } ); 
    return 0;
    
}


int receiveFromClient( int clientSock, char* buffer, Client &client, Mailboxes &mailBoxes )
{
    ssize_t bytesReceieved = recv(clientSock, buffer, protocol::MAX_FRAME, MSG_DONTWAIT);
    if( bytesReceieved < 0 )
    {
        if( errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR )
        {
            logger::logToFile( { logger::LogLevel::ERROR,
                                      "recv failed on sock " + std::to_string( clientSock ) 
                                      + ": " + strerror( errno ) } ); 
            return 0;
        }
    }
    
    if( bytesReceieved == 0 )
    {
        logger::logToFile( { logger::LogLevel::INFO, 
                                   "Connection closed for clientSock: " 
                                   + std::to_string( clientSock ) } ); 
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
            logger::logToFile( { logger::LogLevel::WARNING, 
                                       "Malformed input, discarding " 
                                       + std::to_string( skip ) + " bytes" } ); 
            client.inbox.erase( 0, skip );
            continue;
        }

        
        logger::logToFile( { logger::LogLevel::INFO, 
                                   "Received from sock: " + std::to_string( client.sock ) 
                                   + std::string( newFrame.body )  } ); 

        // We need to check what kind of command is being sent.
        struct logger::Log result;
        auto payload = protocol::resolveCommand( newFrame.body, result ); 
        
        if( payload )
        {
            result = handleCommand( *payload, mailBoxes );
        }
       
        logger::logToFile( { result.logLevel, result.logMsg } );
        client.inbox.erase( 0, newFrame.frameLen );
        if( sendMessage( client.sock, result.logMsg ) < 0 )
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
        std::cerr << "Missing argument <ip port>" << std::endl;
        return 1;   
    }
    
    if( !logger::logInit( "server_" ) )
    {
        std::cerr << "Error opening log file" << std::endl;
        return 1;
    }

    int listenPort = 0;
    int listenSock = -1;

    auto result = std::from_chars(
                argv[1], argv[1] + strlen( argv[1] ), listenPort );

    if( result.ec != std::errc() )
    {
        logger::logToFile( { logger::LogLevel::ERROR, "Bad argument error <ip port>" } );
        return 1;
    }

    // Open socket to listen to, if it fails exit with status 1
    if ( ( listenSock = open_socket( listenPort ) ) < 0 )
    {
        return 1;
    }

    logger::logToFile( { logger::LogLevel::INFO, 
                               "Server is running on port " 
                               + std::to_string( listenPort ) + "!" } ); 

    PolledFds fds;     

    if( listen(listenSock, BACKLOG) < 0 )
    {
        logger::logToFile( { logger::LogLevel::ERROR, 
                                   "Listen failed on port: " + std::string( argv[1] ) } );
        return 1;
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
            
            logger::logToFile( { logger::LogLevel::ERROR, "Poll failed - Closing down" } );
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

    logger::logToFile( { logger::LogLevel::INFO, "Closing server..." } );
    logger::closeLog();
    close( listenSock );
    return 0;
}