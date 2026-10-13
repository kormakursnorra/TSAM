#pragma once

#include <string_view>
#include <optional>
#include <variant>
#include <vector>
#include <string>
#include <array>

namespace protocol
{
    constexpr size_t HEADER_SIZE = 4;     // Size of message header
    constexpr size_t MIN_COMMAND = 10;    // STATUSREQ command
    constexpr size_t MAX_COMMAND = 5000;  // Set capped size for commands (bytes)
    
    // Set minimum valid size for a single message (15 bytes)
    constexpr size_t MIN_FRAME = HEADER_SIZE + MIN_COMMAND + 1; 
    // Set capped size for a single message (5005 bytes)
    constexpr size_t MAX_FRAME = HEADER_SIZE + MAX_COMMAND + 1; 

    // Message delimeters.  
    inline constexpr char SOH = 0x01; // start of header
    inline constexpr char STX = 0x02; // start of text
    inline constexpr char ETX = 0x03; // end of text

    // Message command headers
    inline constexpr std::array<std::string_view, 3> commandNames = {
        "SENDMSG", "GETMSG", "STATUSREQ"
    };

    enum class ParseStatus { Ok, Incomplete, Malformed };

    enum class CommandType { None = -1, SendMsg, GetMsg, StatusReq };

    struct SendMsg   { std::string sender; std::string recipient; std::string body; };
    struct GetMsg    { std::string username; };
    struct StatusReq {};

    using Payload = std::variant< SendMsg, GetMsg, StatusReq >;

    // constexpr std::string_view toString( MessageCommand c ) 
    // {
    //     return msgCommandNames[ static_cast< std::size_t >( c ) ];
    // }

    // constexpr std::optional< MessageCommand > parseCommand( std::string_view s ) 
    // {
    //     for( std::size_t i = 0; i < msgCommandNames.size(); ++i )
    //     {
    //         if ( msgCommandNames[i] == s )
    //             return static_cast< MessageCommand >( i );
    //     }
    //     return std::nullopt;
    // }
    
    // Message frame
    struct Frame 
    {
        ParseStatus status; 
        size_t frameLen;  // bytes to consume from the buffer
        Payload payload;  // the <command> part, between STX and ETX
    };

    protocol::Frame parseMessage( const std::string_view &buf );

    constexpr size_t findNextFrameStart( const std::string_view &buf );

    constexpr int resolveCommand( const std::string_view &command, Payload &payload );

    /*
    This class provides functionality to parse and verify messages from clients. It processes
    the raw data packets into defined Frame structs.
    */
    class MessageHandler
    {
    public:
        

        MessageHandler();    // Constructor
        ~MessageHandler(){}; // Default destructor for class
        /* 
        Parse and unpack a given buffer (message) into a Frame struct to return. 
        */
        
        
    };
};