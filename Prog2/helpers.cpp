#include "helpers.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in.h>


#include <unistd.h>
#include <iostream>
#include <random>
#include <cerrno>



int initializeSocket( const SocketParams& params = SocketParams{} )
{
    int sockfd;
    
    if( ( sockfd = socket( params.domain, params.type, params.protocol ) ) < 0 )
    {
        perror("Error: Failed to create socket");
        close( sockfd );
        return -1;
    }

    struct timeval tv;
    tv.tv_sec = TIMEOUT_MS;
    tv.tv_usec = ( TIMEOUT_MS %  100) * 100;

    if( setsockopt( sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof( tv ) ) < 0 )
    {
        perror( "Error: Couldn't set socket timeout" );
        close( sockfd );
        return -1;
    }

    return sockfd;
}


int sendAndReceive( const int sockfd, std::string data, char* buff, size_t buffSize ) 
{    
    for( int attempt = 0; attempt < MAX_RETRIES; attempt++ )
    {
        ssize_t sent = send( sockfd, data.data(), data.length(), 0 ); 
        

        if( sent < 0 )
        {
            perror( "Error: Couldn't send data\n" );
            return -1;
        }

        ssize_t received = recv( sockfd, buff, buffSize, 0 );
        
         // Check if resource is available or is blocking ( try again ) 
        if( received < 0 )
        {
            if( errno == EAGAIN || errno == EWOULDBLOCK )
            {
                continue;
            } 
            perror( "Error: No response\n" );
            return -1;
        }
        // Port is open, return byte count
        return static_cast< int >( received );
    }
    
    return 0;
}


int mapToPort( const int bytesReceived, const int port, std::map< int, int >& portMap )
{
    for ( const auto& entry : portMap ) 
    {
        const int &key = entry.first;
        if( key == bytesReceived )
        {
            portMap[key] = port;
            return 0;
        }
    }
    return 1;
}




void constructMessage( std::string &secretMessage, uint32_t &secretNumber, const std::string &userNames )
{
    std::random_device rd;
    std::mt19937 gen( rd() ); 
    std::uniform_int_distribution< uint32_t > dist( 0, UINT32_MAX );
    
    secretNumber = dist( gen );
    
    secretMessage.clear();
    secretMessage.append( "S.E.C.R.E.T.:" );
    secretMessage += userNames;

    uint32_t netOrder = htonl(secretNumber); // convert to network byte order
    
    // reinterpret_cast netOrder int value to char* to comply with append parameter
    secretMessage.append( reinterpret_cast< const char* >( &netOrder ), sizeof( netOrder) ); 
}