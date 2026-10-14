#include "message_protocol.h"

#include <netinet/in.h>
#include <ratio>
#include <string_view>
#include <sys/types.h>
#include <optional>
#include <iostream>
#include <ostream>
#include <cstdint>
#include <cstdio>


namespace protocol
{

std::string createFrame(const std::string_view& message)
{
    uint16_t len = static_cast< uint16_t >( message.size() + 5 ); 
    uint16_t netLen = htons(len);

    std::string frame;
    frame.reserve(len);

    frame.push_back(protocol::SOH);
    frame.append(reinterpret_cast< const char* >( &netLen ), sizeof( netLen ) );
    frame.push_back(protocol::STX);
    frame.append(message.data(), message.size());
    frame.push_back(protocol::ETX);

    return frame;
}


Frame parseMessage( const std::string_view &message )
{    
    if( message.empty() )
    {
        std::cerr << "Received empty buffer" << std::endl;
        return { FrameStatus::Incomplete, 0, "" };
    }

    if( message[0] != SOH )
    {
        std::cerr << "First character NOT <SOH> (0x01)" << std::endl;
        return { FrameStatus::Malformed, 0, "" };
    }
    
    if( message.size() < HEADER_SIZE )
    {
        std::cerr << "Incomplete message (message < HEADER_SIZE)" << std::endl;
        return { FrameStatus::Incomplete, 0, "" };
    }
    
    if ( message[3] != STX ) 
    {
        std::cerr << "Missing character <STX> (0x02)" << std::endl;
        return { FrameStatus::Malformed, 0, "" };
    }
    
    // Extract <length> field. Since it's in big-endian we need to perform bitwise
    // left-shifting to move the first byte to the right position 
    size_t firstByte  = size_t( static_cast< uint8_t >( message[1] ) << 8 );
    size_t secondByte = size_t( static_cast< uint8_t >( message[2] ) );
    
    size_t length = firstByte | secondByte;

    if( length < MIN_FRAME || length > MAX_FRAME )
    {
        std::cerr << "Value of <length> not within MIN_FRAME or MAX_FRAME" << std::endl;
        return { FrameStatus::Malformed, 0, "" };
    }

    if( message.size() < length || length > message.size() )
    {
        std::cerr << "Size of buffer not the same as <length>" << std::endl;
        return { FrameStatus::Incomplete, 0, "" };
    }

    if( message[ length - 1 ] != ETX )
    {
        std::cerr << "Last character NOT <ETX> (0x03)" << std::endl;
        return { FrameStatus::Malformed, 0, "" };
    } 
    
    return Frame{
                FrameStatus::Ok, 
                length, 
                message.substr(HEADER_SIZE, length - HEADER_SIZE - 1 ) };
}

// Returns the index of the next plausible frame start, or message.size() if none.
size_t findNextFrameStart( const std::string_view &message ) 
{
    size_t nextFramePos = message.find( SOH, 1 );
    if( nextFramePos == std::string_view::npos )
    {
        return message.size();
    }
    return nextFramePos;
}

constexpr std::string_view nextField( const std::string_view &body, size_t& startPos )
{
    if ( startPos > body.size() ) {
        return {}; // ran out of fields
    }
    size_t comma = body.find(',', startPos);
    std::string_view field = body.substr(startPos, comma - startPos);
    
    if( comma == std::string_view::npos )
    {
        startPos = body.size() + 1;
    }
    else {
        startPos = comma + 1;
    }
    
    return field;
}

std::optional< Payload > resolveCommand( const std::string_view &body )
{
    if( body.empty() )
    {
        return std::nullopt;
    }

    size_t pos = 0;
    std::string_view name = nextField( body, pos );

    if( name == "SENDMSG" )
    {
        auto sender = nextField( body, pos );
        auto recipient = nextField( body, pos );
        if( sender.empty() || recipient.empty() )
        {
            return std::nullopt;
        }

        const auto contentsStart = pos;
        auto contents = body.substr( contentsStart );
        if( contents.empty() )
        {
            return std::nullopt;
        }

        return SendMsg{ std::string( sender ), std::string( recipient ), std::string( contents ) };
    }

    if( name == "GETMSG" )
    {
        auto username = nextField( body, pos );
        if( username.empty() )
        {
            return std::nullopt;
        }
        return GetMsg{ std::string( username ) };
    }

    if( name == "STATUSREQ" )
    {
        return StatusReq{};
    }

    return std::nullopt;
}

};