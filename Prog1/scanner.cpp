#include <cerrno>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <memory.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <cstdlib>
#include <vector>


const int MAX_RETRIES = 5;
const int TIMEOUT_MS = 500;

static int sockfd; // UDP socket

// client- and server socket addresses
static struct sockaddr_in destaddr; 
static struct sockaddr_in srcaddr;

static std::vector< uint8_t > openPorts; 
/* Sets the receive timout for the UDP socket,
it is in milliseconds(ms)

inputs: 
ms: timeout duration in milliseconds.
Return:
0 if the timeout is set successfully.
-1 if an error occurs.

*/
int setSocketTimeout( int ms )
{
    struct timeval tv;
    tv.tv_sec = ms / 1000;
    tv.tv_usec = ( ms % 1000 ) * 1000;

    return setsockopt( sockfd, SOL_SOCKET, 
                    SO_RCVTIMEO, &tv, 
                    sizeof( tv ) );
}

/*
The scanPort function scans a single UPD port by sending data 
to the destination and waiting for a response. The function will try
MAX_RETRIES times if no response is recieved
inputs:
port: the UDP port number to scan
data: the data that is sent to the destination.
Return:
1 if a response is recieved and the port is considered open.
0 if no response is recieved after all retries.
-1 if an error occurs while sending or receiving data.
*/
int scanPort( const int port, std::string data ) 
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
            // Check if resource is available or is blocking ( try again ) 
            if( errno == EAGAIN || errno == EWOULDBLOCK )
            {
                continue;
            }
            
            perror( "Error: No response\n" );
            return -1;
        }
        
        // Port is open, return 1
        return 1;
    }
    
    return 0;
}
/*
The Main function reads the IP Address and port range from
 the command line arguments, it creates the UDP socket and sets 
 the socket timeout, and scan each port in the specified range.
inputs:
argc : number of command line arguments.
argv: command line arguments containing the IP Address, 
lowest port and highest port
 Return: 
 0 when the program finishes successfully.
*/
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

    std::string data = "Hello World!"; // data being sent

    // Iterate over port range
    for( int port=loPort; port <= hiPort; port++ )
    {
        int result = scanPort( port, data ); 

        if ( result < 0 )
        {
            std::cerr << "Error: Couldn't scan port: " << port << std::endl;
            continue;
        }

        if( result == 1 )
        {
            std::cout << "Port " << port << " is open" << std::endl;
            openPorts.push_back( port );
        }
    }

    close( sockfd );

    return 0;
}
