#include <netinet/ip6.h>
#include <netinet/udp.h>
#include <netinet/ip.h>
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
#include <bitset>
#include <cerrno>
#include <string>
#include <vector>
#include <random>
#include <array>
#include <map>


const int MAX_RETRIES = 5;
const int TIMEOUT_MS = 1000;


struct Signature 
{
    uint8_t groupId;
    uint32_t secretSigil;
};

struct SecretData 
{
    int secretPort;
    int hiddenPort;
    Signature signature;
};

struct GuardianData 
{
    int guardianPort;
    Signature signature;
    std::string secretSpell;
    struct ip6_hdr responseHdr;
    struct udphdr responseUdpHdr;  
};

struct EvilBitData 
{
    int evilPort;
    int HiddenPort;
    struct ip ipv4Hdr;
    struct udphdr udpHdr;
    Signature signature;  
};

/* 
Sets the receive timout for the UDP socket n milliseconds(ms)
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
Maps the port numbers to the expected byte count 
of their respective greetings. A matching key gets
the value of the port number that produced it.
inputs:
bytesReceived: The number of bytes the port greeted with
port: The UDP port number that sent the reply
portMap: map of expected response size -> port number
return:
0 if the byte count matched a port number and the port was recorded
1 if no key matched the byte count
*/
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


/*
The sendToPort function scans a single UPD port by sending data 
to the destination and waiting for a response. The function will try
MAX_RETRIES times if no response is recieved
inputs:
sockfd: connected UDP socket to send on
data: the data that is sent to the destination.
buff: buffer the reply is written into
buffSize: the capacity of buff
Return:
>0(number of bytes received) if a response is recieved and the port is considered open.
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
        
        // Port is open, return byte count
        return static_cast< int >( received );
    }
    
    return 0;
}

/* 
constructMessage build the initial S.E.C.R.E.T. handshake message:
the literal prefix, the group members and a freshly generated 
32-bit random number appended as four raw bytes in network.
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


void buildIpv6Buffer( std::vector< uint8_t >& buff, const struct ip6_hdr& iphdr, struct udphdr udphdr, 
            const char* payload, size_t payloadLen )
{
    const uint8_t* src = reinterpret_cast< const uint8_t* >( &iphdr.ip6_src );
    const uint8_t* dst = reinterpret_cast< const uint8_t* >( &iphdr.ip6_dst );
    buff.insert( buff.end(), src, src + 16 );
    buff.insert( buff.end(), dst, dst + 16 );

    uint32_t upperLen = htonl( static_cast< uint32_t >( sizeof( udphdr ) + payloadLen ) );
    const uint8_t* ulenBytes = reinterpret_cast< const uint8_t* >( &upperLen );
    buff.insert( buff.end(), ulenBytes, ulenBytes + 4 );

    buff.push_back( 0 ); buff.push_back( 0 ); buff.push_back( 0 );
    buff.push_back( IPPROTO_UDP );

    udphdr.uh_sum = 0; // checksum field must be zero while computing
    const uint8_t* uh = reinterpret_cast< const uint8_t* >( &udphdr );
    buff.insert( buff.end(), uh, uh + sizeof( udphdr ) );

    buff.insert( buff.end(), payload, payload + payloadLen );
    if( buff.size() % 2 != 0 ) 
    {
        buff.push_back( 0 ); // pad to even length
    }
}

void buildIpv4Buffer( std::vector< uint8_t >& buff, const struct ip& ipv4hdr )
{
    const uint8_t* src = reinterpret_cast< const uint8_t* >( &ipv4hdr.ip_src );
    const uint8_t* dst = reinterpret_cast< const uint8_t* >( &ipv4hdr.ip_dst );

    buff.insert( buff.end(), src, src + 4 );
    buff.insert( buff.end(), dst, dst + 4 );

    uint32_t upperLen = htonl( static_cast< uint32_t >( sizeof( udphdr ) ) );
    const uint8_t* ulenBytes = reinterpret_cast< const uint8_t* >( &upperLen );
    buff.insert( buff.end(), ulenBytes, ulenBytes + 4 );

    buff.push_back( 0 ); buff.push_back( 0 ); buff.push_back( 0 );
    buff.push_back( IPPROTO_UDP );

    if( buff.size() % 2 != 0 ) 
    {
        buff.push_back( 0 ); // pad to even length
    }
}


int checksum( std::vector< uint8_t > buff )
{
    uint32_t sum = 0;
    for( size_t i = 0; i < buff.size(); i += 2 )
    {
        sum += ( static_cast< uint16_t >( buff[i] ) << 8 ) | buff[i + 1];
    }

    while( sum >> 16 )
    {
        sum = ( sum & 0xFFFF ) + ( sum >> 16 );
    }

    uint16_t result = static_cast< uint16_t >( ~sum );
    if( result == 0 ) 
    {
        result = 0xFFFF; // RFC 8200: zero checksum is invalid, use all-ones
    }

    return htons( result );
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
    if( connect( sockfd, reinterpret_cast< struct sockaddr* >( &destaddr ), sizeof( destaddr ) )  < 0 )
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

    secretData.signature.groupId = static_cast< uint8_t >( static_cast< unsigned char >( reply[0] ) );
    
    uint32_t challengeNumber;
    memcpy( &challengeNumber, &reply[1], sizeof( challengeNumber ) );

    secretData.signature.secretSigil = secretNumber ^ ntohl( challengeNumber );

    uint32_t sigilNetOrder = htonl( secretData.signature.secretSigil );
    
    std::string newMessage;
    newMessage.resize( 1 + sizeof( uint32_t ) );
    newMessage[0] = static_cast< char >( secretData.signature.groupId );
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
    const size_t payloadLen = 1 + sizeof( uint32_t );              
    const size_t udpLen     = sizeof( struct udphdr ) + payloadLen;
    const size_t totalLen   = sizeof( struct ip6_hdr ) + udpLen;

    std::string newMessage;
    newMessage.resize( totalLen );
    
    struct ip6_hdr replyHdr = guardianData.responseHdr;
    replyHdr.ip6_dst = guardianData.responseHdr.ip6_src;
    replyHdr.ip6_src = guardianData.responseHdr.ip6_dst;
    replyHdr.ip6_plen = htons( static_cast< uint16_t >( udpLen ) );
    
    memcpy(&newMessage[0], &replyHdr, sizeof( replyHdr ) );
    
    struct udphdr replyUdpHdr = 
    {
        guardianData.responseUdpHdr.uh_dport,
        guardianData.responseUdpHdr.uh_sport,
        htons( static_cast< uint16_t >( udpLen ) ),
        0
    };
    
    char payload[5];
    payload[0] = static_cast< char >( guardianData.signature.groupId );
    uint32_t sigilNetOrder = htonl( guardianData.signature.secretSigil );
    memcpy( &payload[1], &sigilNetOrder, sizeof( sigilNetOrder ) );
    
    std::vector< uint8_t > buff;
    buildIpv6Buffer( buff, replyHdr, replyUdpHdr, payload, payloadLen );
    replyUdpHdr.uh_sum = checksum( buff );  

    memcpy( &newMessage[0], &replyHdr, sizeof( replyHdr ) );
    memcpy( &newMessage[ sizeof( replyHdr ) ], &replyUdpHdr, sizeof( replyUdpHdr ) );
    memcpy( &newMessage[ sizeof( replyHdr ) + sizeof( replyUdpHdr ) ], &payload, payloadLen );
    
    destaddr.sin_port = htons( guardianData.guardianPort );
    if( connect( sockfd, reinterpret_cast< struct sockaddr* >( &destaddr ), sizeof( destaddr ) ) < 0 )
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
    size_t textStart = 0;
    while( textStart < static_cast< size_t >( bytesReceived ) &&
        !( std::isprint( static_cast< unsigned char >( buffer[textStart] ) ) ||
            buffer[textStart] == '\n' ) )
    {
        textStart++;
    }

    std::string fullResponse( buffer + textStart, bytesReceived - textStart );
    size_t firstQuote = fullResponse.find( '"' );
    size_t secondQuote = fullResponse.find( '"', firstQuote + 1 );
    
    if( firstQuote == std::string::npos || secondQuote == std::string::npos )
    {
        std::cerr << "Error: couldn't find secret spell in reply: " << fullResponse << std::endl;
        return 1;
    }

    guardianData.secretSpell = fullResponse.substr( firstQuote + 1, secondQuote - firstQuote - 1 );
    return 0; 
}   


int solveEvilPort( const int sockfd, struct sockaddr_in& destaddr, EvilBitData& evilBitData )
{
    const size_t payloadLen = 5;
    const size_t udpLen = sizeof(struct udphdr ) + payloadLen;
    const size_t hdrLen = sizeof( struct ip ) + udpLen;

    int rawSockfd;
    if( ( rawSockfd = socket( AF_INET, SOCK_RAW, IPPROTO_UDP ) ) < 0 )
    {
        perror("Error: Failed to create RAW socket aarggh!! Socket is NOT RAW >:( !!!!");
        close( rawSockfd );
        exit( 1 );
    }
    
    if( setSocketTimeout( TIMEOUT_MS, rawSockfd) < 0 )
    {
        perror( "Error: Couldn't set socket timeout" );
        close( rawSockfd );
        exit( 1 );
    }

    destaddr.sin_port = htons( evilBitData.evilPort );
    if( connect( sockfd, reinterpret_cast< struct sockaddr* >( &destaddr ), sizeof( destaddr ) ) < 0 )
    {
        perror("Error: Failed to establish connection with receiver" );
        return 1;
    }


    struct sockaddr_in local;
    socklen_t len = sizeof(local);
    getsockname(sockfd, (struct sockaddr*)&local, &len);


    evilBitData.ipv4Hdr = 
    {
        5,
        IPVERSION,
        0,
        //htons( hdrLen ),
        //htons( 0 ),
        //htons( IP_RF ), // evil bit hehehe
        static_cast< uint16_t >( hdrLen ),
        0,
        IP_RF,
        64,
        IPPROTO_UDP,
        0,
        local.sin_addr.s_addr,
        destaddr.sin_addr.s_addr,
    };

    evilBitData.udpHdr.uh_sport = local.sin_port;

    evilBitData.udpHdr.uh_dport = htons( evilBitData.evilPort );
    evilBitData.udpHdr.uh_ulen = htons( static_cast< uint16_t >( udpLen ) );
    evilBitData.udpHdr.uh_sum = 0;  

    char payload[5];

    payload[0] = static_cast< char >( evilBitData.signature.groupId );

    uint32_t sigilNetOrder = htonl( evilBitData.signature.secretSigil );

    memcpy( &payload[1], &sigilNetOrder, sizeof( sigilNetOrder ) );



    std::vector< uint8_t > udpBuff;

    // Source IPv4 
    const uint8_t* src = reinterpret_cast< const uint8_t* >( &evilBitData.ipv4Hdr.ip_src );
    udpBuff.insert( udpBuff.end(), src, src + 4 );

    // Destination IPv4
    const uint8_t* dst = reinterpret_cast< const uint8_t* >( &evilBitData.ipv4Hdr.ip_dst );
    udpBuff.insert( udpBuff.end(), dst, dst + 4 );

    // Zero 
    udpBuff.push_back( 0 );

    // Protocol
    udpBuff.push_back( IPPROTO_UDP );

    // UDP length
    uint16_t udpLenNet = htons( static_cast< uint16_t >( udpLen ) );
    const uint8_t* ulenBytes = reinterpret_cast< const uint8_t* >( &udpLenNet );
    udpBuff.insert( udpBuff.end(), ulenBytes, ulenBytes + 2 );

    // UDP header
    const uint8_t* udpBytes = reinterpret_cast< const uint8_t* >( &evilBitData.udpHdr );
    udpBuff.insert( udpBuff.end(), udpBytes, udpBytes + sizeof( evilBitData.udpHdr ) );

    // Payload
    udpBuff.insert( udpBuff.end(), reinterpret_cast< const uint8_t* >( payload ), reinterpret_cast< const uint8_t* >( payload ) + payloadLen );

    //checksum
    if ( udpBuff.size() % 2 != 0 )
    {
        udpBuff.push_back( 0 );
    }

    evilBitData.udpHdr.uh_sum = checksum( udpBuff );



    //////////////

    evilBitData.ipv4Hdr.ip_sum = 0;
    std::vector< uint8_t > ipBuff(sizeof(evilBitData.ipv4Hdr));

    memcpy( ipBuff.data(), &evilBitData.ipv4Hdr, sizeof( evilBitData.ipv4Hdr));
    evilBitData.ipv4Hdr.ip_sum = checksum( ipBuff );



    ////////////


    
    destaddr.sin_port = htons( evilBitData.evilPort );
    if( connect( rawSockfd, reinterpret_cast< struct sockaddr* >( &destaddr ), sizeof( destaddr ) ) < 0 )
    {
        perror("Error: Failed to establish connection with receiver" );
        return 1;
    }

    int one = 1;
    if( ( setsockopt(rawSockfd, IPPROTO_IP, IP_HDRINCL, &one, sizeof( one ) ) ) < 0 )
    {
        perror("Error: Failed to set socket option" );
        return 1;
    }
////////
    std::string message;
    message.resize( hdrLen );
    size_t offset = 0;

    memcpy( &message[ offset ], &evilBitData.ipv4Hdr, sizeof( evilBitData.ipv4Hdr));

    offset += sizeof( evilBitData.ipv4Hdr );
    memcpy( &message[ offset ], &evilBitData.udpHdr, sizeof( evilBitData.udpHdr ) );
    offset += sizeof( evilBitData.udpHdr );
    memcpy( &message[ offset ], &payload, payloadLen );

//////
    

ssize_t sent = send( rawSockfd, message.data(), message.length(), 0 );
    if( sent < 0 )
    {
        perror( "Error: Failed to send evil packet\n" );
        close( rawSockfd );
        return 1;
    }

    char buffer[2048];
    ssize_t bytesReceived = recv( sockfd, buffer, sizeof( buffer ) - 1 , 0 );

    if( bytesReceived < 0 )
    {
        perror( "Error: No response from Evil port" );
        close( rawSockfd );
        return 1; 
    }

    buffer[ bytesReceived ] = '\0';
    std:: string replyText( buffer, bytesReceived );
    std::cout << "Evil port response: " << replyText << std::endl;
    size_t colonPos = replyText.rfind( ':' );

    if ( colonPos == std::string::npos )
    {
        std::cerr << "Error: Couldn't find port number in reply: " << replyText << std::endl;
        close( rawSockfd );
        return 1;
    }

    evilBitData.HiddenPort = std::stoi( replyText.substr( colonPos + 1 ) );

    std::cout << "Hidden port is: " << evilBitData.HiddenPort << std::endl;

    close( rawSockfd );
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
    std::vector< int > openPorts = { 
        atoi( argv[2] ), 
        atoi( argv[3] ), 
        atoi( argv[4] ),
        atoi( argv[5] ) 
    };


    int sockfd; // UDP socket
    struct sockaddr_in destaddr = {}; // Server address 

    destaddr.sin_family = AF_INET;

    if( ( sockfd = socket( AF_INET, SOCK_DGRAM, 0 ) ) < 0 )
    {
        perror( "Error: Socket couldn't be created\n" );
        close( sockfd );
        exit( 1 );
    }

    if( setSocketTimeout( TIMEOUT_MS, sockfd) < 0 )
    {
        perror( "Error: Couldn't set socket timeout" );
        close( sockfd );
        exit( 1 );
    }

    // Set the address with the given IP addr.
    if( ( inet_pton( AF_INET, ipaddr, &destaddr.sin_addr ) ) < 1 )
    {
        std::cerr << "Error: Invalid IP addres or address family\n " << ipaddr << std::endl;
        close( sockfd );
        exit( 1 );
    } 

    // Declare the necessary data structures for solving ports.

    SecretData secretData = {};
    GuardianData guardianData = {};
    EvilBitData evilBitData = {};

    // key-value: bytes received - port
    std::map< int, int > portMap = {
        { 614, -1 }, // dragon
        { 184, -1 }, // evil port
        { 404, -1 }, // guardian
        { 1107, -1 } // secret
    };

    // send a light-weight packet to each port and map their response to a key-value
    // for later use
    for( const auto& port : openPorts )
        {
        destaddr.sin_port = htons( port );
        if( connect( sockfd, reinterpret_cast< struct sockaddr* >( &destaddr ), sizeof( destaddr ) )  < 0 )
        {
            perror("Error: Failed to establish connection with receiver" );
            close( sockfd );
            exit( 1 );
        }
        
        char buffer[2048];
        int bytesReceived = sendToPort( sockfd, "Hello!", buffer, sizeof( buffer ) - 1);

        if( bytesReceived < 0 )
        {
            std::cerr << "Error: Couldn't scan port: " << port << std::endl;
            close( sockfd );
            exit( 1 );
        }

        buffer[ bytesReceived ] = '\0';

        if( mapToPort( bytesReceived, port, portMap) != 0 )
        {
            std::cerr << "Error: Couln't map to port: " << port << std::endl;
            close( sockfd );
            exit( 1 );
        }
        
        // check if current port is guardian port
        if( port == portMap.at( 404 ) )
        {
            // copy the raw binary structure at the front of response
            memcpy(&guardianData.responseHdr, buffer, sizeof( guardianData.responseHdr ) );
            memcpy( &guardianData.responseUdpHdr, buffer + sizeof( guardianData.responseHdr ),
             sizeof( guardianData.responseUdpHdr ) );
        }
    }

    secretData.secretPort = portMap.at( 1107 );
    if( solveSecretPort( sockfd, destaddr, secretData) != 0 )
    {
        std::cerr << "Error: Secret Port couldn't be solved" << std::endl;
        close( sockfd );
        exit( 1 );   
    }

    guardianData.signature = secretData.signature;
    guardianData.guardianPort = portMap.at( 404 );
    if( solveGuardianPort( sockfd, destaddr, guardianData) != 0 )
    {
        std::cerr << "Error: Guardian Port couldn't be solved" << std::endl;
        close( sockfd );
        exit( 1 );
    }

    evilBitData.evilPort = portMap.at( 184 );
    evilBitData.signature = secretData.signature;
    if( solveEvilPort( sockfd, destaddr, evilBitData ) != 0 )
    {
        std::cerr << "Error: Evil Port couldn't be solved" << std::endl;
        close( sockfd );
        exit( 1 );
    } 


    close( sockfd );

    return 0;
}
