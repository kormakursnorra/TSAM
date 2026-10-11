#include "message_protocol.h"
#include <sstream>

MessageHandler::MessageHandler(){}

int MessageHandler::parseMessage( char* buffer )
{
    std::vector<std::string> tokens;
    std::string token;

    // Split command from client into tokens for parsing
    std::stringstream stream( buffer );

    while( stream >> token )
    {
        tokens.push_back( token );
    }

    // Handle the case when the message is > 5000 bytes
    if( atoi( tokens.back().c_str() ) != ETX )  
    {
        return 1;
    }

    return 1;
}