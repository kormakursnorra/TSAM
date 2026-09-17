#include <netinet/ip6.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <memory.h>
#include <unistd.h>
#include <cstdlib>
#include <stdio.h>

#include <iostream>
#include <iomanip>
#include <ostream>
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
const int TIMEOUT_MS = 30;

struct SecretData {
    int secretPort;
    int hiddenPort;
    uint8_t groupId;
    uint32_t secretSigil;
};

struct GuardianData {
    int guardianPort;
    uint8_t groupId;
    uint32_t secretSigil;
    struct ip6_hdr responseHdr;
};

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

*/
int mapToPort( char* buff, const int port, std::map< std::string, int >& portMap )
{
    std::string buffContent =  static_cast< std::string >( buff );
    for ( const auto& entry : portMap ) 
    {
        const std::string &key = entry.first;
        if( buffContent.find( key ) != std::string::npos )
        {
            portMap[key] = port;
            return 0;
        }
    }

    return 1;
}


/*
The sendToPort function scans a single UPD port by sending data 
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
int sendToPort( const int sockfd, std::string data, char* buff, size_t buffSize ) 
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
        return static_cast< int >( received );
    }
    
    return 0;
}

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


int solveSecretPort( const int sockfd, struct sockaddr_in& destaddr, SecretData& secretData )
{
    uint32_t secretNumber;     // Randomly generated, 32-bit secret number  
    const std::string userNames = "aroni21, bergurpb24, kormakur24"; // Usernames
    std::string secretMessage; // The "message" (or packet) being sent

    if( constructMessage(secretNumber, secretMessage, userNames) < 0 )
    {
        perror("Error: Failed to construct message");
        return 1;
    }

    destaddr.sin_port = htons( secretData.secretPort );
    if( connect( sockfd, reinterpret_cast< struct sockaddr* >( &destaddr ), 
    sizeof( destaddr ) )  < 0 )
    {
        perror("Error: Failed to establish connection with receiver" );
        return 1;
    }

    char reply[5];
    // expecting response: group ID + challenge number
    int bytesReceived = sendToPort( sockfd, secretMessage, reply, sizeof( reply ) );
    if( bytesReceived != static_cast< int >( sizeof( reply ) ) )
    {
        std::cerr << "Error (1): Bad reply from S.E.C.R.E.T. port " << secretData.secretPort
                << " (" << bytesReceived << " bytes)" << std::endl;
        return 1;
    }

    secretData.groupId = static_cast< uint8_t >( static_cast< unsigned char >( reply[0] ) );
    
    uint32_t challengeNumber;
    memcpy( &challengeNumber, &reply[1], sizeof( challengeNumber ) );

    secretData.secretSigil = secretNumber ^ ntohl( challengeNumber );

    uint32_t sigilNetOrder = htonl( secretData.secretSigil );
    
    std::string newMessage;
    newMessage.resize( 1 + sizeof( uint32_t ) );
    newMessage[0] = static_cast< char >( secretData.groupId );
    memcpy( &newMessage[1], &sigilNetOrder, sizeof( sigilNetOrder ) );

    char buffer[2048];
    // expected reponse: reveal hidden port
    bytesReceived = sendToPort( sockfd, newMessage, buffer, sizeof( buffer ) - 1 );
    if( bytesReceived < 0 )
    {
        std::cerr << "Error (2): Bad reply from S.E.C.R.E.T. port " << secretData.secretPort
                << " (" << bytesReceived << " bytes)" << std::endl;
        return 1;
    }

    buffer[ bytesReceived ] = '\0';

    std::string replyText( buffer );
    size_t colonPos = replyText.rfind( ':' );
    if( colonPos == std::string::npos )
    {
        std::cerr << "Error: couldn't find port number in reply: " << replyText << std::endl;
        return 1;
    }

    secretData.hiddenPort = std::stoi( replyText.substr( colonPos + 1 ) );
    return 0;
}

int solveGuardianPort( const int sockfd, struct sockaddr_in& destaddr, GuardianData& guardianData )
{
    
    std::string newMessage;
    newMessage.resize( sizeof( guardianData.responseHdr ) + 1 + sizeof( uint32_t ) );
    
    struct ip6_hdr replyHdr;
    memcpy( &replyHdr, &guardianData.responseHdr, sizeof( guardianData.responseHdr ) );
    
    replyHdr.ip6_dst = guardianData.responseHdr.ip6_src;
    replyHdr.ip6_src = guardianData.responseHdr.ip6_dst;

    // std::string hdrString;

    // const char* un1 = reinterpret_cast< const char* >( &replyHdr.ip6_ctlun.ip6_un1 );
    // const char* un2 = reinterpret_cast< const char* >( &replyHdr.ip6_ctlun.ip6_un2_vfc );

    // const char* dst = reinterpret_cast< const char* >( &replyHdr.ip6_dst );
    // const char* src = reinterpret_cast< const char* >( &replyHdr.ip6_src );

    // hdrString.append( un1, sizeof( un1 ) );
    // hdrString.append( un2, sizeof( un2 ) );
    // hdrString.append( dst, sizeof( dst ) );
    // hdrString.append( src, sizeof( src ) );

    newMessage.append( reinterpret_cast< const char* >( &replyHdr ), sizeof( replyHdr ) );
    uint32_t sigilNetOrder = htonl( guardianData.secretSigil );
    memcpy( &newMessage[ sizeof( guardianData.responseHdr ) ], &guardianData.groupId, 1 );
    memcpy( &newMessage[1], &sigilNetOrder, sizeof( sigilNetOrder ) );

    destaddr.sin_port = htons( guardianData.guardianPort );
    if( connect( sockfd, reinterpret_cast< struct sockaddr* >( &destaddr ), 
        sizeof( destaddr ) ) < 0 )
    {
        perror("Error: Failed to establish connection with receiver" );
        return 1;
    }
    
    char buffer[2048];
    int bytesReceived = sendToPort( sockfd, newMessage, buffer, sizeof( buffer) );
    if( bytesReceived < 0 )
    {
        std::cerr << "Error (1): Bad reply from Guardian port " << guardianData.guardianPort
                << " (" << bytesReceived << " bytes)" << std::endl;
        return 1;
    } 
    
    buffer[ bytesReceived ] = '\0';
    std::cout << bytesReceived << buffer << std::endl;

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

    std::map< std::string, int > portMap = {
        {"D.R.A.G.O.N", -1},
        {"evil port", -1},
        {"the guardian", -1},
        {"S.E.C.R.E.T.:", -1}
    }; 

    SecretData secretData;
    GuardianData guardianData;

    // send a light-weight packet to each port and map their response to a key-value
    // for later use
    for( const auto& port : openPorts )
    {
        destaddr.sin_port = htons( port );
        if( connect( sockfd, reinterpret_cast< struct sockaddr* >( &destaddr ), 
        sizeof( destaddr ) )  < 0 )
        {
            perror("Error: Failed to establish connection with receiver" );
            exit( 1 );
        }
        
        char buffer[2048];
        int bytesReceived = sendToPort( sockfd, "Hello!", buffer, sizeof( buffer ) );

        if( bytesReceived < 0 )
        {
            std::cerr << "Error: Couldn't scan port: " << port << std::endl;
            exit( 1 );
        }

        buffer[ bytesReceived ] = '\0';

        if( mapToPort( buffer, port, portMap) != 0 )
        {
            std::cerr << "Error: Couln't map to port: " << port << std::endl;
            exit( 1 );
        }
        
        // check if current port is guardian port
        if( port == portMap.at( "the guardian" ) )
        {
            // copy the raw binary structure at the front of response
            memcpy(&guardianData.responseHdr, buffer, sizeof( guardianData.responseHdr ) );
        }
    }

    secretData.secretPort = portMap.at( "S.E.C.R.E.T.:" );
    if( solveSecretPort( sockfd, destaddr, secretData) != 0 )
    {
        std::cerr << "Error: Secret Port couldn't be solved" << std::endl;
        exit( 1 );   
    }

    guardianData.guardianPort = portMap.at("the guardian");
    if( solveGuardianPort( sockfd, destaddr, guardianData) != 0 )
    {
        std::cerr << "Error: Guardian Port couldn't be solved" << std::endl;
        exit( 1 );
    }




    close( sockfd );

    return 0;
}
