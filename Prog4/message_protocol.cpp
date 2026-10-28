#include "message_protocol.h"
#include "protocol_types.h"
#include "shared_util.h"

#include <netinet/in.h>
#include <string_view>
#include <sys/types.h>
#include <charconv>
#include <optional>
#include <cstdint>
#include <cstdio>
#include <ctime>


namespace protocol
{

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

std::optional< Servers > parseServerList( const std::string_view &list )
{
    if( list.empty() )
    {
        // TODO: Log here?
        return std::nullopt;
    }
    
    Servers result;
    size_t start = 0;
    
    while( start < list.size() )
    {
        // Isolate single entry (';' seperated)
        const size_t semicolon = list.find(';', start );
        const size_t endPos = semicolon == std::string_view::npos 
                                ? std::string_view::npos : semicolon - start;
        const auto entry = list.substr( start, endPos );

        // Handle sub-fields within entry. 
        // Find all commas
        const size_t firstComma = entry.find(',');
        if( firstComma == std::string_view::npos )
        {
            // TODO: Log here?
            return std::nullopt; 
        }
        const size_t secondComma = entry.find(',', firstComma + 1 );
        if( secondComma == std::string_view::npos )
        {
            // TODO: Log here?
            return std::nullopt;
        }

        // Isolate Group Id field.
        const auto groupId = entry.substr( 0, firstComma );
        if( groupId.empty() )
        {
            return std::nullopt;
        }

        // Isolate IP address field.
        const auto ip = entry.substr( 
            firstComma + 1, secondComma - firstComma - 1 );
        
        if( ip.empty() )
        {
            return std::nullopt;
        }

        // Isolate port number field.
        const auto portText = entry.substr(secondComma + 1);
        int16_t port = util::validatePortNum( portText );
        if( port < 0 )
        {
            return std::nullopt;
        }

        result.serverList.emplace_back( 
            std::string( groupId ), 
            std::string( ip ), 
            port 
        );

        if ( semicolon == std::string_view::npos )
        {
            break;
        }

        start = semicolon + 1;
        if (start == list.size())
        { 
            break;
        }
    }
    
    return result;
};

std::optional< StatusResp > parseStatusRespList( const std::string_view &list )
{
    if( list.empty() )
    {
        return std::nullopt;
    }

    StatusResp result;
    size_t start = 0;

    while( start < list.size() )
    {
        // Isolate single entry (',' seperated)
        const size_t comma = list.find( ',', start );
        const size_t endPos = comma == std::string_view::npos 
                            ? std::string_view::npos : comma - start;
        const auto entry = list.substr( start, endPos );
        
        // Handle sub-fields within entry. 
        // Find colon position 
        const size_t colon = entry.find( ':' );
        if( colon == std::string_view::npos )
        {
            // TODO: Log here?
            return std::nullopt; 
        }

        // Isolate name
        const auto name = entry.substr( 0, colon );
        if( name.empty() )
        {
            return std::nullopt;
        }

        // Isolate count
        const auto countText = entry.substr( colon + 1 );
        if( countText.empty() )
        {
            return std::nullopt;
        }

        uint8_t count = 0;
        const auto [end, error] = std::from_chars(
                    countText.data(), countText.data() + countText.size(), count );
        
        
        if( error != std::errc() || end != countText.data() + countText.size() )
        {
            return std::nullopt;
        }

        result.serverMsgCount.emplace( 
            std::string( name ), count 
        );

        if ( comma == std::string_view::npos )
        {
            break;
        }

        start = comma + 1;
        if (start == list.size())
        { 
            break;
        }
    }

    return result;
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
            // TODO: Log here: log_note
            return std::nullopt;
        }

        return Helo{ std::string( fromGroupId ) };
    }

    if( name == SERVERS )
    {
        auto servers = parseServer( body.substr( pos ) );
        if( !servers )
        {
            return std::nullopt;
        }
        return servers.value();

    }

    if( name == KEEPALIVE )
    {
        uint8_t msgCount; 
        auto msgCountText = nextField( body, pos );
        if( msgCountText.empty() )
        {
            return std::nullopt;
        }
        const auto [end, error] = std::from_chars(
                msgCountText.data(), msgCountText.data() + msgCountText.size(), msgCount);

        if( error != std::errc() || end != msgCountText.data() + msgCountText.size() )
        {
            return std::nullopt;
        }

        return KeepAlive { msgCount };
    }

    if( name == GETMSGS )
    {
        auto groupId = nextField( body, pos );
        if( groupId.empty() )
        {
            // TODO: Log here: log_note
            return std::nullopt;
        }
        return GetMsg{ std::string( groupId ) };
    }

    if( name == SENDMSG )
    {
        auto fromGroupId = nextField( body, pos );
        auto toGroupId = nextField( body, pos );
        auto timestampText = nextField( body, pos );
        if( fromGroupId.empty() || toGroupId.empty() ||  timestampText.empty() )
        {
            // TODO: Log here: log_note
            return std::nullopt;
        }

        int64_t timestamp = util::convertTimestamp( timestampText );
        if( timestamp < 0 )
        {
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

        return SendMsg{ std::string( fromGroupId ),
                        std::string( toGroupId ), 
                        timestamp,
                        std::string( contents ) };
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

    if( name == STATUSRESP )
    {
        auto statusresp = parseStatusResp( body.substr( pos ) );
        if( !statusresp )
        {
            return std::nullopt;
        }
        return statusresp.value();
    }

    // TODO: Log here: DROP
    return std::nullopt;
}

};