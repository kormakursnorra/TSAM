#include "message_protocol.h"
#include "protocol_types.h"

#include <netinet/in.h>
#include <string_view>
#include <sys/types.h>
#include <optional>
#include <iomanip>
#include <sstream>
#include <cstdint>
#include <cstdio>
#include <chrono>
#include <ctime>


namespace protocol
{

int64_t getTimestamp()
{
    const auto timestamp = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>( timestamp.time_since_epoch()).count();
}

std::string createFrame( const std::string_view& message )
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
        return { FrameStatus::Incomplete, 0, "" };
    }

    if( message[0] != SOH )
    {
        // TODO: Log here: DROP
        return { FrameStatus::Malformed, 0, "" };
    }
    
    if( message.size() < HEADER_SIZE )
    {
        // TODO: Log here: log_note
        return { FrameStatus::Incomplete, 0, "" };
    }
    
    if ( message[3] != STX ) 
    {
        // TODO: Log here: DROP
        return { FrameStatus::Malformed, 0, "" };
    }
    
    // Extract <length> field. Since it's in big-endian we need to perform bitwise
    // left-shifting to move the first byte to the right position 
    size_t firstByte  = size_t( static_cast< uint8_t >( message[1] ) << 8 );
    size_t secondByte = size_t( static_cast< uint8_t >( message[2] ) );
    
    size_t length = firstByte | secondByte;

    if( length < MIN_FRAME || length > MAX_FRAME )
    {
        return { FrameStatus::Malformed, 0, "" };
    }

    if( message.size() < length )
    {
        return { FrameStatus::Incomplete, 0, "" };
    }

    if( message[ length - 1 ] != ETX )
    {
        // TODO: Log here: DROP
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
        // TODO: Log here: log_note
        return std::nullopt;
    }

    size_t pos = 0;
    std::string_view name = nextField( body, pos );

    if( name == HELO )
    {
        auto fromGroupId = nextField( body, pos );
        if( fromGroupId.empty() )
        {
            return std::nullopt;
        }

        return Helo{ std::string( fromGroupId ) };
    }

    if( name == SENDMSG )
    {
        auto sender = nextField( body, pos );
        auto recipient = nextField( body, pos );

        if( sender.empty() || recipient.empty() )
        {
            // TODO: Log here: log_note
            return std::nullopt;
        }

        if( pos > body.size() )
        {
            // TODO: Log here: log_note
            return std::nullopt;
        }

        const auto contentsStart = pos;
        auto contents = body.substr( contentsStart );
        if( contents.empty() )
        {
            // TODO: Log here: log_note
            return std::nullopt;
        }

        return SendMsg{ std::string( sender ),
                        std::string( recipient ), 
                        std::string( contents ) };
    }

    if( name == GETMSGS )
    {
        auto username = nextField( body, pos );
        if( username.empty() )
        {
            // TODO: Log here: log_note
            return std::nullopt;
        }
        return GetMsg{ std::string( username ) };
    }

    if( name == STATUSREQ )
    {
        if( !nextField( body, pos ).empty() )
        {
            // TODO: Log here: log_note
            return std::nullopt;
        }
        return StatusReq{};
    }

    // TODO: Log here: DROP
    return std::nullopt;
}

};