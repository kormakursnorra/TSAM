#include <cerrno>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <memory.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <cstdlib>
#include <thread>

static int sockfd; // UDP socket
static std::string data = "Hello World!"; // data being sent
    
// client- and server socket addresses
static struct sockaddr_in destaddr; 
static struct sockaddr_in srcaddr;

const int COPIES = 3;
const int MAX_RETRIES = 5;
const int TIMEOUT_MS = 500;

int setSocketTimeout( int ms )
{
    struct timeval tv;
    tv.tv_sec = ms / 1000;
    tv.tv_usec = ( ms % 1000 ) * 1000;

    return setsockopt( sockfd, SOL_SOCKET, 
                    SO_RCVTIMEO, &tv, 
                    sizeof( tv ) );
}


int scanPort( const int port ) 
{
    // Set the port
    destaddr.sin_port = htons(port);
    
    char buffer[2048];
    
    for( int attempt = 0; attempt < MAX_RETRIES; attempt++ )
    {

        ssize_t sent = sendto( sockfd, data.data(), data.length(), 0,
                        ( struct sockaddr* )&destaddr, sizeof( destaddr ) ); 
        
        if( sent < 0 )
        {
            perror( "Error: Couldn't send data\n" );
            return -1;
        }

        socklen_t srcaddrlen = sizeof( srcaddr );

        ssize_t received = recvfrom( sockfd, buffer, sizeof( buffer ), 0,
                            ( struct sockaddr* )&srcaddr, &srcaddrlen );

        if( received < 0 )
        {
            if( errno == EAGAIN || errno == EWOULDBLOCK )
            {
                continue;
            }
            
            perror( "Error: No response\n" );
            return -1;
        }


        
        buffer[ received ] = '\0';
        std::cout << "Port " << port << " reply (" << received << " bytes): " << buffer << std::endl; 
        return 1;
    }
    
    return 0;
}

int main( int argc, char* argv[] )
{
    if( argc < 4 )
    {
        perror( "Error: Insufficient arguments\n");
        exit( 1 );
    }

    const char *ipaddr = argv[1];
    const int  loPort = atoi( argv[2] );
    const int  hiPort = atoi( argv[3] );

    destaddr.sin_family = AF_INET;

    if( ( sockfd = socket( AF_INET, SOCK_DGRAM, 0 ) ) < 0 )
    {
        perror( "Error: Socket couldn't be created\n" );
        exit( 1 );
    }

    if( setSocketTimeout( TIMEOUT_MS) < 0 )
    {
        perror( "Error: Couldn't set socket timeout" );
        exit( 1 );
    }

    // Set the address with the given IP addr.
    if( ( inet_pton( AF_INET, ipaddr, &destaddr.sin_addr ) ) < 1 )
    {
        std::cerr << "Error: Invalid IP addres or address family\n " << ipaddr << std::endl;
        exit( 1 );
    }

    // Iterate over port range
    for( int port=loPort; port <= hiPort; port++ )
    {
        int result = scanPort( port ); 

        if ( result < 0 )
        {
            std::cerr << "Error: Couldn't scan port: " << port << std::endl;
            continue;
        }

        if( result == 1 )
        {
            std::cout << "Port " << port << " is open" << std::endl;
        }
    }

    close( sockfd );

    return 0;
}
