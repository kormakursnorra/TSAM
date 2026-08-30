#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <stdio.h>
#include <cstdlib>
#include <thread>
#include <memory.h>


static int sockfd; // UDP socket
std::string data = "Hello World!"; // data being sent
    
// client- and server socket addresses
static struct sockaddr_in destaddr; 
static struct sockaddr_in srcaddr;

const int COPIES = 3;
const int MAX_RETRIES = 5;
const int TIMEOUT_MS = 200;


int scanPort( const int port ) 
{
    // Set the port
    destaddr.sin_port = htons(port);
    
    socklen_t srcaddrlen;
    char buffer[2048];
    int retVal;

    for( int attempt = 0; attempt < MAX_RETRIES; attempt++ )
    {
        if( ( retVal = sendto( sockfd, data.data(), data.length(), 0,
            ( struct sockaddr* )&destaddr, sizeof( destaddr ) ) ) )
        {
            perror( "Error: Couldn't send data" );
            return -1;
        }

        if (attempt < COPIES - 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT_MS));
        }

        if( ( retVal = recvfrom( sockfd, buffer, sizeof( buffer ), 0,
            ( struct sockaddr* )&srcaddr, &srcaddrlen ) ) < 0 )
        {
            perror( "Error: No response" );
            return -1;
        } 
    }
    
    std::cout << "Received, buffer data: " << buffer << std::endl;
    
    return 0;
}

int main( int argc, char* argv[] )
{
    if( argc < 4 )
    {
        perror( "Error: Insufficient arguments");
        exit( 1 );
    }

    const char *ipaddr = argv[1];
    const int  loPort = atoi( argv[2] );
    const int  hiPort = atoi( argv[3] );

    destaddr.sin_family = AF_INET;

    if( ( sockfd = socket( AF_INET, SOCK_DGRAM, 0 ) ) < 0 )
    {
        perror( "Error: Socket couldn't be created" );
        exit( 1 );
    }

    // Set the address with the given IP addr.
    if( ( inet_pton( AF_INET, ipaddr, &destaddr.sin_addr ) ) < 1 )
    {
        std::cerr << "Error: Invalid IP addres or address family " << ipaddr << std::endl;
        exit( 1 );
    }

    // Iterate over port range
    for( int port=loPort; port <= hiPort; port++ )
    {
        if ( scanPort( port ) < 0 )
        {
            std::cerr << "Error: Couldn't scan port: " << port << std::endl;
            continue;
        }

        std::cout << "Data Received; Port " << port << "is open" << std::endl;
       
    }

    return 0;
}
