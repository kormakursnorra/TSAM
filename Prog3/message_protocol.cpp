#include "message_protocol.h"

#include <optional>
#include <string_view>
#include <sys/types.h>
#include <iostream>
#include <ostream>
#include <cstdint>
#include <cstdio>


namespace protocol
{

Frame parseMessage( const std::string_view &buf )
{    
    if( buf.empty() )
    {
        std::cerr << "Received empty buffer" << std::endl;
        return { FrameStatus::Incomplete };
    }

    if( buf[0] != SOH )
    {
        std::cerr << "First character NOT <SOH> (0x01)" << std::endl;
        return { FrameStatus::Malformed };
    }
    
    if( buf.size() < HEADER_SIZE )
    {
        std::cerr << "Incomplete message (buf < HEADER_SIZE)" << std::endl;
        return { FrameStatus::Incomplete };
    }
    
    if ( buf[3] != STX ) 
    {
        std::cerr << "Missing character <STX> (0x02)" << std::endl;
        return { FrameStatus::Malformed };
    }
    
    // Extract <length> field. Since it's in big-endian we need to perform bitwise
    // left-shifting to move the first byte to the right position 
    size_t firstByte  = size_t( static_cast< uint8_t >( buf[1] ) << 8 );
    size_t secondByte = size_t( static_cast< uint8_t >( buf[2] ) );
    
    size_t length = firstByte | secondByte;

    if( length < MIN_FRAME || length > MAX_FRAME )
    {
        std::cerr << "Value of <length> not within MIN_FRAME or MAX_FRAME" << std::endl;
        return { FrameStatus::Malformed };
    }

    if( buf.size() < length || length > buf.size() )
    {
        std::cerr << "Size of buffer not the same as <length>" << std::endl;
        return { FrameStatus::Incomplete };
    }

    if( buf[ length - 1 ] != ETX )
    {
        std::cerr << "Last character NOT <ETX> (0x03)" << std::endl;
        return { FrameStatus::Malformed };
    } 
    
    return Frame{
                FrameStatus::Ok, 
                length, 
                buf.substr(HEADER_SIZE, length - HEADER_SIZE - 1 ) };
}

// Returns the index of the next plausible frame start, or buf.size() if none.
size_t findNextFrameStart( const std::string_view &buf ) 
{
    size_t nextFramePos = buf.find( SOH, 1 );
    if( nextFramePos == std::string_view::npos )
    {
        return buf.size();
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
    size_t pos = 0;
    std::string_view name = nextField( body, pos );

    if( name == "SENDMSG" )
    {
        auto sender    = nextField( body, pos );
        auto recipient = nextField( body, pos );
        auto contents = body.substr( pos );
        if( pos > body.size() || sender.empty() || recipient.empty() || contents.empty() ) 
        { 
            return std::nullopt; 
        }
        return SendMsg{ 
            std::string( sender ), 
            std::string( recipient ), 
            std::string( contents ) };  
    }
    
    if( name == "GETMSG" )
    {
        auto username = nextField( body, pos);
        if( pos > body.size() || username.empty() ) 
        { 
            return std::nullopt; 
        }
        return GetMsg{ std::string( username ) };
    }

    if( name == "STATUSREQ" && pos > body.size() )
    {
        return StatusReq{};
    }

    return std::nullopt;
}

};