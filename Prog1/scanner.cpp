#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <iostream>
#include <stdio.h>
#include <cstdlib>

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

    // create UDP socket
    int sockfd;
    if( ( sockfd = socket( AF_INET, SOCK_DGRAM, 0 ) ) < 0 )
    {
        perror( "Error: Socket couldn't be created" );
        exit( 1 );
    }

    std::string data = "Hello World!";
    // Set up server address 
    struct sockaddr_in destaddr; 
    destaddr.sin_family = AF_INET;

    // Iterate over port range
    for( int port=loPort; port <= hiPort; port++ )
    {
        destaddr.sin_port = htons(port);
        // Set the address with the given IP addr.
        if( inet_pton( AF_INET, ipaddr, &destaddr.sin_addr ) )
        {
            std::cerr << "Error: Invalid IP addres or address family" << ipaddr << std::endl;
            exit( 1 );
        }
        
        struct sockaddr_in srcaddr;
        socklen_t srcaddrlen;
        char buffer[2048];
        int ret;
        
        while( ( ret = sendto( sockfd, data.c_str(), data.length(), 0,
            ( struct sockaddr* )&destaddr, sizeof( destaddr ) ) ) )
        {

            if( ( ret = sendto( sockfd, data.c_str(), data.length(), 0,
                ( struct sockaddr* )&destaddr, sizeof( destaddr ) ) ) )
            {
                perror( "Error: Couldn't send data" );
                exit( 1 );
            }
            
            if( ( ret = recvfrom( sockfd, buffer, sizeof( buffer ), 0,
                ( struct sockaddr* )&srcaddr, &srcaddrlen ) ) < 0 )
            {
                perror( "Error: No response" );
                exit( 1 );
            }
            std::cout << "Received: " << buffer << std::endl;
        }   
    }

    return 0;
}
