#include "tsamserver.h"


int main( int argc, char* argv[] )
{
    // Process command line argument/s
    
    if( argc != 2 )
    {
        perror("Missing argument <ip port>\n");
        exit(1);   
    }
    
    int listenPort = 0;
    if( ( listenPort = atoi( argv[1] ) ) == 0 )
    {
        perror("Bad argument error <ip port>\n");
        exit(1);
    }

    // Create listen socket
    int listenSock = open_socket( listenPort );
}