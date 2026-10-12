#include "message_protocol.h"

#include <optional>
#include <string_view>
#include <sys/types.h>
#include <iostream>
#include <ostream>
#include <cstdint>
#include <cstdio>

inline constexpr std::string_view nextField( const std::string_view &command, size_t& startPos )
{
    if ( startPos > command.size() ) {
        return {}; // ran out of fields
    }
    size_t comma = command.find(',', startPos);
    std::string_view field = command.substr(startPos, comma - startPos);
    
    if( comma == std::string_view::npos )
    {
        startPos = command.size() + 1;
    }
    else {
        startPos = comma + 1;
    }
    
    return field;
}

proto::MessageHandler::MessageHandler(){}

int proto::MessageHandler::parseMessage( const std::string_view &buf, struct Frame& newFrame )
{    
    if( buf.empty() )
    {
        std::cerr << "Received empty buffer" << std::endl;
        return 1;
    }

    if( buf[0] != SOH )
    {
        std::cerr << "First character NOT <SOH> (0x01)" << std::endl;
        return 1;
    }
    
    if( buf.size() < HEADER_SIZE )
    {
        std::cerr << "Incomplete message (buf < HEADER_SIZE)" << std::endl;
        return 1;
    }
    
    if ( buf[3] != STX ) 
    {
        std::cerr << "Missing character <STX> (0x02)" << std::endl;
        return 1;
    }
    
    // Extract <length> field. Since it's in big-endian we need to perform bitwise
    // left-shifting to move the first byte to the right position 
    size_t firstByte  = size_t( static_cast< uint8_t >( buf[1] ) << 8 );
    size_t secondByte = size_t( static_cast< uint8_t >( buf[2] ) );
    
    size_t length = firstByte | secondByte;

    if( length < MIN_FRAME || length > MAX_FRAME )
    {
        std::cerr << "Value of <length> not within MIN_FRAME or MAX_FRAME" << std::endl;
        return 1;
    }

    if( buf.size() < length || length > buf.size() )
    {
        std::cerr << "Size of buffer not the same as <length>" << std::endl;
        return 1;
    }

    if( buf[ length - 1 ] != ETX )
    {
        std::cerr << "Last character NOT <ETX> (0x03)" << std::endl;
        return 1;
    } 

    // We need to check what kind of command is being sent.
    auto command = buf.substr( STX, length - 1 );
    Payload payload; 

    if( resolveCommand( command, payload ) > 0 )
    {
        std::cerr << "Unable to resolve command" << std::endl;
        return 1;
    }

    newFrame = Frame{ length, payload };

    return 0;
}

// Returns the index of the next plausible frame start, or buf.size() if none.
size_t proto::MessageHandler::findNextFrameStart( const std::string_view &buf, struct Frame &newFrame ) 
{
    for (size_t i = 1; i < buf.size(); ++i) 
    {
        if ( buf[i] == SOH && parseMessage( buf.substr( i ), newFrame ) != 1 )
        {
            return i;
        }
    }
    return buf.size();
}

int proto::MessageHandler::resolveCommand( const std::string_view &command, Payload &payload )
{
    
    CommandType commandName = CommandType::None; 
    for( auto i = 0U; i < commandNames.size(); ++i )
    {
        if( command.find( commandNames[i], 0 ) != std::string_view::npos )
        {
            commandName = static_cast< CommandType >( i );
        }
    }

    switch( commandName )
    {
        case CommandType::SendMsg:
        {
            // Unpack command (<from>,<to>,<message>) 
            size_t pos = commandNames[0].size() + 1;
            auto sender    = nextField( command, pos );
            auto recipient = nextField( command, pos );
            if( pos > command.size() ) 
            { 
                break; 
            }
            auto body = command.substr( pos );
            payload = SendMsg{ 
                std::string( sender ), 
                std::string( recipient ), 
                std::string( body ) };
            break;   
        }
        case CommandType::GetMsg:
        {
            size_t pos = commandNames[1].size() + 1;
            if( pos > command.size() ) 
            { 
                break; 
            }
            payload = GetMsg{ std::string( nextField( command, pos ) ) };
            break;
        }
        case CommandType::StatusReq:
        {
            payload = StatusReq{};
            break;
        }
        default:
        {
            std::cerr << "No valid command name found in given <command>" << std::endl;
            return 1;
        }
    }

    return 0;
    

}

int proto::MessageHandler::sendMessage()
{

}