#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <memory.h>
#include <unistd.h>
#include <cstdlib>
#include <stdio.h>

#include <iostream>

#include <iterator>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cerrno>
#include <string>
#include <vector>
#include <random>
#include <array>
#include <map>


const int MAX_RETRIES = 5;
const int TIMEOUT_MS = 10;

/* Consturcts a "secret message" as a data packet and 
 to send to the open ports that request it. 

*/
int constructMessage( uint32_t &secretNumber, std::string &secretMessage, const std::string &userNames)
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
    return 0;
}

/* Sets the receive timout for the UDP socket,
it is in milliseconds(ms)

inputs: 
ms: timeout duration in milliseconds.
sockfd: socket to set the timeout on.
Return:
0 if the timeout is set successfully.
-1 if an error occurs.

*/
int setSocketTimeout( int ms, const int sockfd )
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
int sendToPort( const int sockfd, const int port, std::string data ) 
{    
    char buffer[2048];
    
    for( int attempt = 0; attempt < MAX_RETRIES; attempt++ )
    {


        ssize_t sent = send( sockfd, data.data(), data.length(), 0 ); 
        
        if( sent < 0 )
        {
            perror( "Error: Couldn't send data\n" );
            return -1;
        }

        ssize_t received = recv( sockfd, buffer, sizeof( buffer ), 0 );

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
        buffer[ received ] = '\0';
        std::cout << "Port " << port << " reply (" << received << " bytes): " << buffer << std::endl; 
        for (int i = 0; i < received; i++) {
            printf("%02x ", (unsigned char)buffer[i]);
        }
        std::cout << std::endl;
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
    if( argc < 6 )
    {
        perror( "Error: Insufficient arguments\n");
        exit( 1 );
    }

    const char *ipaddr = argv[1];
    const std::array< int, 4 > openPorts = { 
        atoi( argv[2] ), 
        atoi( argv[3] ), 
        atoi( argv[4] ),
        atoi( argv[5] ) 
    };


    int sockfd; // UDP socket
    struct sockaddr_in destaddr; // Server address 

    destaddr.sin_family = AF_INET;

    if( ( sockfd = socket( AF_INET, SOCK_DGRAM, 0 ) ) < 0 )
    {
        perror( "Error: Socket couldn't be created\n" );
        exit( 1 );
    }

    if( setSocketTimeout( TIMEOUT_MS, sockfd) < 0 )
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
   
    uint32_t secretNumber;     // Randomly generated, 32-bit secret number  
    const std::string userNames = "aroni21, bergurpb24, kormakur24"; // Usernames
    std::string secretMessage; // The "message" (or packet) being sent

    if( constructMessage(secretNumber, secretMessage, userNames) < 0 )
    {
        perror("Error: Failed to construct message");
        exit( 1 );
    }
    
    destaddr.sin_port = htons( openPorts.at(3) );
    if( connect( sockfd, reinterpret_cast< struct sockaddr* >( &destaddr ), 
                sizeof( destaddr ) )  < 0 )
    {
        perror("Error: Failed to establish connection with receiver" );
        exit( 1 );
    }

    int result = sendToPort( sockfd, openPorts.at(3), secretMessage );
    if ( result < 0 )
    {
        std::cerr << "Error: Couldn't scan port: " << openPorts.at(3) << std::endl;
    }


    
    // if( result == 1 )
    // {
    //     std::cout << "Port " << port << " is open" << std::endl;
    // }


    close( sockfd );

    return 0;
}
