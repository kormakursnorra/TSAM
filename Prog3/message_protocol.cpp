#include "message_protocol.h"
#include "logger.h"

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


Frame parseMessage( const std::string_view &message, std::string &errMsg )
{   
    if( message.empty() )
    {
        return { FrameStatus::Incomplete, 0, "" };
    }

    if( message[0] != SOH )
    {
        logger::logToFile( { logger::LogLevel::ERROR,
                                   "First character NOT <SOH> (0x01)" } );
        return { FrameStatus::Malformed, 0, "" };
    }
    
    if( message.size() < HEADER_SIZE )
    {
        logger::logToFile( { logger::LogLevel::WARNING,
                                   "Incomplete message (message < HEADER_SIZE)" } );
        return { FrameStatus::Incomplete, 0, "" };
    }
    
    if ( message[3] != STX ) 
    {
        logger::logToFile( { logger::LogLevel::ERROR,
                                   "Missing character <STX> (0x02)" } );
        return { FrameStatus::Malformed, 0, "" };
    }
    
    // Extract <length> field. Since it's in big-endian we need to perform bitwise
    // left-shifting to move the first byte to the right position 
    size_t firstByte  = size_t( static_cast< uint8_t >( message[1] ) << 8 );
    size_t secondByte = size_t( static_cast< uint8_t >( message[2] ) );
    
    size_t length = firstByte | secondByte;

    if( length < MIN_FRAME || length > MAX_FRAME )
    {
        logger::logToFile( { logger::LogLevel::ERROR, 
                                   "Value of <length> not within MIN_FRAME or MAX_FRAME" } );
        return { FrameStatus::Malformed, 0, "" };
    }

    if( message.size() < length || length > message.size() )
    {
        logger::logToFile( { logger::LogLevel::WARNING, 
                                   "Size of buffer not the same as <length>" } );
        return { FrameStatus::Incomplete, 0, "" };
    }

    if( message[ length - 1 ] != ETX )
    {
        logger::logToFile( { logger::LogLevel::ERROR, 
                                   "Last character NOT <ETX> (0x03)" } );
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

std::optional< Payload > resolveCommand( const std::string_view &body, struct logger::Log &log )
{
    if( body.empty() )
    {
        log = { logger::LogLevel::WARNING, 
                  "Command body is empty" };
        return std::nullopt;
    }

    uint8_t count = 0;
    size_t pos = 0;
    std::string_view name = nextField( body, pos );

    if( name == "SENDMSG" )
    {
        auto sender = nextField( body, pos );
        auto recipient = nextField( body, pos );

        if( sender.empty() || recipient.empty() )
        {
            log = { logger::LogLevel::WARNING, 
                      "Missing sender and/or recipient" };
            return std::nullopt;
        }

        const auto contentsStart = pos;
        auto contents = body.substr( contentsStart );
        if( contents.empty() )
        {
            log = { logger::LogLevel::WARNING, 
                      "Missing message contents" };
            return std::nullopt;
        }

        return SendMsg{ std::string( sender ),
                        std::string( recipient ), 
                        std::string( contents ) };
    }

    if( name == "GETMSG" )
    {
        auto username = nextField( body, pos );
        if( username.empty() )
        {
            log = { logger::LogLevel::WARNING, 
                      "Missing username" };
            return std::nullopt;
        }
        return GetMsg{ std::string( username ) };
    }

    if( name == "STATUSREQ" )
    {
        if( !nextField( body, pos ).empty() )
        {
            log = { logger::LogLevel::WARNING, 
                      "STATUSREQ must have empty body" };
            return std::nullopt;
        }
        return StatusReq{};
    }

    log = { logger::LogLevel::ERROR, 
              "Unknown command" };
    return std::nullopt;
}

};