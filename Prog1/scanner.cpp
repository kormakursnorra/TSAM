#include <sys/socket.h>
#include <netinet/in.h>
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
        perror( "Error creating socket" );
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
        if( inet_pton( AF_INET, ipaddr, &destaddr.sin_addr)
        {
            std::cerr << "invalid ip addres or address family" << ipaddr << std::endl;
            exit( 1 );
        }
    }


    int ret;
    if( ( ret = sendto( sockfd, s.c_str(), s.length(), 0,
        ( struct sockaddr* )&destaddr, sizeof( destaddr ) ) )
    {
        perror( "Error sending" );
        exit( 1 );
    }

    struct sockaddr_in srcaddr;
    socklen_t srcaddrlen;
    char buffer[2048];

    if( ( ret = recvfrom( sockfd, buffer, sizeof( buffer ), 0,
        ( struct sockaddr* )&srcaddr, &srcaddrlen ) ) < 0 )
    {
        perror( "Error receiving" );
        exit( 1 );
    }

    std::cout << "received: " << buffer << std::endl;
    return 0;
}
