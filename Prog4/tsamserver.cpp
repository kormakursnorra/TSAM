#include "message_protocol.h"
#include "tsamserver.h"
#include "logger.h"
#include "log_event.h"
#include <netinet/in.h>
#include <system_error>
#include <algorithm>
#include <charconv>
#include <iostream>
#include <ostream>
#include <variant>
#include <fcntl.h>
#include <string>
#include <cerrno>


/* Sets non-blocking on socket */
static bool setNonBlocking( int fd )
{
    int flags = fcntl( fd, F_GETFL, 0 );
    if( flags < 0 )
    {
        return false;
    }
    return fcntl( fd, F_SETFL, flags | O_NONBLOCK ) == 0;
}

int open_socket( int portnum )
{
    struct sockaddr_in sk_addr;   // address settings for bind()
    int sock;                     // socket opened for this port
    int set = 1;                  // for setsockopt

    
    if( ( sock = socket(AF_INET, SOCK_STREAM, 0) ) < 0 )
    {
        logger::logToFile( { logger::LogLevel::ERROR, "Failed to open socket" } );
        return -1;
    }

    if( !setNonBlocking( sock ) )
    {
        logger::logToFile( { logger::LogLevel::ERROR,
                                    std::string( "Failed to set O_NONBLOCK: " ) + strerror( errno ) } );
        close( sock );
        return -1;
    }

    // Turn on SO_REUSEADDR to allow socket to be quickly reused after 
    // program exit.

    if(setsockopt( sock, SOL_SOCKET, SO_REUSEADDR, &set, sizeof( set )) < 0 )
    {
       logger::logToFile( { logger::LogLevel::ERROR, "Failed to set SO_REUSEADDR" } );
    }
    set = 1;

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
    logger::logToFile( { logger::LogLevel::INFO, 
                                "Client closed connection: " + 
                                std::to_string( clientSocket ) } );

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
                // Client might still be connected, so try sending again
                continue;
            }

            logger::logToFile( { logger::LogLevel::ERROR, "Error occured when sending" } );
            return -1;
        }

        if( bytesSent == 0 ) // client is gone
        {
            logger::logToFile( { logger::LogLevel::ERROR, "Failed to send a message." } ); 
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
            return { logger::LogLevel::INFO, "EMPTY" };
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
        
        if( response.size() > protocol::MAX_COMMAND )
        {
            return { logger::LogLevel::WARNING, "Size of reply is equal to or exceeds maximum" };
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

#ifdef SO_NOSIGPIPE   // macOS/BSD only; Linux relies on MSG_NOSIGNAL in send()
    int on = 1;
    if( setsockopt( clientSock, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof( on ) ) < 0 )
    {
        logger::logToFile( { logger::LogLevel::ERROR,
                             std::string( "Failed to set SO_NOSIGPIPE: " ) + strerror( errno ) } );
        close( clientSock );
        return -1;
    }
#endif

    struct timeval tv = { .tv_sec = 2, .tv_usec = 0 };
    if ( setsockopt(clientSock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0 )
    {
        logger::logToFile( { logger::LogLevel::ERROR, "Error setting socket option to SO_SNDTIMEO" } ); 
        close( clientSock );
        return -1;
    }

    // Add new client to the list of fds being polled
    fds.push_back( { clientSock, POLLIN, 0 } );

    // Create a new client to store information. Re-use the record for this socket if we have seen it
    // before so we don't use any stale record for a reused fd number.
    clients.insert_or_assign( clientSock, Client( clientSock ) );

    std::string logMsg = "Client connected: " + std::to_string( clientSock ) + " from " 
            + inet_ntoa( client.sin_addr ) + ":"  + std::to_string( ntohs( client.sin_port ) ); 

    logger::logToFile( { logger::LogLevel::INFO, logMsg } ); 
    return 0;
    
}

static bool replyOwed( const protocol::Payload &payload,
                       const connection::Connection &conn )
{
    using namespace protocol;

    const bool isPeer = ( conn.kind == connection::ConnectionKind::Peer );

    // A status request always gets an answer, from a client or a peer.
    if( std::holds_alternative< StatusReq >( payload ) ) return true;

    // SENDMSG: our client gets "Message sent"; a peer gets nothing.
    if( std::holds_alternative< SendMsg >( payload ) ) return !isPeer;

    // GETMSG: our client gets its oldest message, one reply.
    // A peer must get SENDMSG frames instead, which is not built yet,
    // so for now it gets nothing rather than the wrong format.
    if( std::holds_alternative< GetMsg >( payload ) ) return !isPeer;

    // KEEPALIVE, HELO, SERVERS, STATUSRESP: acted on, never replied to.
    return false;
}

// Won't work until client identification is complete
int receiveFromClient( int clientSock, char* buffer, connection::Connection &client,
                       protocol::Mailboxes &mailBoxes )
{
    ssize_t bytesReceieved = recv(clientSock, buffer, protocol::MAX_FRAME, MSG_DONTWAIT);
    if( bytesReceieved < 0 )
    {
        if( errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR )
        {
            return 0;
        }
        logger::logToFile( { logger::LogLevel::ERROR,
                                   "recv failed on sock " + std::to_string( clientSock ) 
                                  + ": " + strerror( errno ) } ); 
        return 1;
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
            // More bytes might be incoming so try again
            break;
        }
        
        if ( newFrame.status == protocol::FrameStatus::Malformed )
        {
            size_t skip = protocol::findNextFrameStart( client.inbox );
            log_event( "DROP", client.remoteIp.c_str(), client.remotePort,
                        client.peerGroupId.c_str(), "malformed frame" ); 
            client.inbox.erase( 0, skip );
            continue;
        }

                // newFrame.body points into client.inbox, so copy it before erasing.
        const std::string body( newFrame.body );
        client.inbox.erase( 0, newFrame.frameLen );

        auto payload = protocol::resolveCommand( body );

        if( !payload )
        {
            // Not a command we accept: DROP, with the start of it as evidence.
            // Only the first part is logged, so junk floods can't fill the disk.
            constexpr size_t DROP_DETAIL_MAX = 200;
            std::string detail = "unrecognised command: " + body.substr( 0, DROP_DETAIL_MAX );
            if( body.size() > DROP_DETAIL_MAX )
            {
                detail += "... (" + std::to_string( body.size() ) + " bytes)";
            }
            log_event_bytes( "DROP", client.remoteIp.c_str(), client.remotePort,
                             client.peerGroupId.c_str(), detail.data(), detail.size() );

            // Only our own client is told; peers and unidentified connections get silence.
            if( client.kind == connection::ConnectionKind::OwnClient )
            {
                const std::string reply = "Unknown command";
                if( sendMessage( client.socket, reply ) < 0 )
                {
                    return 1;
                }
                log_event_bytes( "SEND", client.remoteIp.c_str(), client.remotePort,
                                 client.peerGroupId.c_str(), reply.data(), reply.size() );
            }
            continue;
        }

        // Logged before peerGroupId is set below, so the RECV line of a HELO
        // shows "-" as the group id, as log_event.h requires.
        log_event_bytes( "RECV", client.remoteIp.c_str(), client.remotePort,
                         client.peerGroupId.c_str(), body.data(), body.size() );

        // The first accepted command decides what is at the other end.
        // A compliant peer must start with HELO; our own client never sends it.
        if( client.kind == connection::ConnectionKind::Unknown )
        {
            if( const auto *helo = std::get_if< protocol::Helo >( &*payload ) )
            {
                client.kind        = connection::ConnectionKind::Peer;
                client.peerGroupId = helo->fromGroupId;
                log_note( "%s:%u identified itself as %s", client.remoteIp.c_str(),
                          static_cast< unsigned >( client.remotePort ),
                          helo->fromGroupId.c_str() );
            }
            else
            {
                // Temporary until client identification exists: see todo.md.
                client.kind = connection::ConnectionKind::OwnClient;
                log_note( "%s:%u sent no HELO; treating it as our client",
                          client.remoteIp.c_str(),
                          static_cast< unsigned >( client.remotePort ) );
            }
        }

        // Act on the command whether or not a reply goes back
        // (a peer's SENDMSG is still stored).
        struct logger::Log result = handleCommand( *payload, mailBoxes );

        if( replyOwed( *payload, client ) )
        {
            if( sendMessage( client.socket, result.logMsg ) < 0 )
            {
                return 1;
            }
            log_event_bytes( "SEND", client.remoteIp.c_str(), client.remotePort,
                             client.peerGroupId.c_str(),
                             result.logMsg.data(), result.logMsg.size() );
        }
        // No else: not replying is the behaviour, and the RECV line is the record.
    }

    return 0;
}

int main( int argc, char* argv[] )
{
    // Process command line argument/s
    
    if( argc < 2 )
    {
        std::cerr << "Missing argument <port>" << std::endl;
        return 1;   
    }

    // This is just for the optional parameter of allowing the current date to be added the 
    // log file name.
    if( argc > 2 && argc != 3 )
    {
        std::cerr << "Arguments allowed: <port><logDate>" << std::endl;
        return 1;
    }
    
    if( !logger::logInit( "server", argv[2] ) )
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
        logger::logToFile( { logger::LogLevel::ERROR, "Bad argument error <port>" } );
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
            // if interrupted by signal, continue
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