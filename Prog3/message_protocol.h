#pragma once

#include "logger.h"

#include <string_view>
#include <optional>
#include <variant>
#include <vector>
#include <string>
#include <array>

namespace protocol
{
    /* Protocol for messages: <SOH><length><STX><command><ETX> */ 

    constexpr size_t HEADER_SIZE = 4;     // Size of message header
    constexpr size_t MAX_COMMAND = 5000;  // Set capped size for commands (bytes)
    
    // Set minimum valid size for a single message (15 bytes)
    constexpr size_t MIN_FRAME = HEADER_SIZE + 1; 
    // Set capped size for a single message (5005 bytes)
    constexpr size_t MAX_FRAME = HEADER_SIZE + MAX_COMMAND + 1; 

    // Message delimeters.  
    inline constexpr char SOH = 0x01; // start of header
    inline constexpr char STX = 0x02; // start of text
    inline constexpr char ETX = 0x03; // end of text

    /* Enum values represent the state of a given message:
     * Ok         -> Valid message sent. 
     * Incomplete -> Missing data.
     * Malformed  -> Not protocol formatted. 
     */
    enum class FrameStatus { Ok, Incomplete, Malformed };

    // Structs that represent/wrap the command types/data. 
    struct SendMsg   { std::string sender; std::string recipient; std::string contents; };
    struct GetMsg    { std::string username; };
    struct StatusReq {};

    using Payload = std::variant< SendMsg, GetMsg, StatusReq >;
    
    // Wrapper for the information concerning a message frame.
    struct Frame 
    {
        FrameStatus status;    // status of the frame after parsing (Ok, Incomplete, Malformed) 
        size_t frameLen;       // bytes to consume from the buffer
        std::string_view body; // the <command> part, between STX and ETX
    };

    /*
     * Creates a protocol compliant frame for the given message.
     */
    std::string createFrame( const std::string_view &message );

    /* 
     * Validates the given message if its protocol compliant, i.e. missing fields, data misalignment etc.
     */
    Frame parseMessage( const std::string_view &message );

    /* 
     * Finds the start of the next frame from a given buffer. 
     */
    size_t findNextFrameStart( const std::string_view &message );

    /*
     * Validates a given command and if it follows the following command formats:
     * 1. SENDMSG,<from>,<to>,<message>.
     * 2. GETMSG,<username>.
     * 3. STATUSREQ
     */
    std::optional< Payload > resolveCommand( const std::string_view &command, struct logger::Log &log );
};