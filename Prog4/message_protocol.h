#pragma once

#include "protocol_types.h"

#include <string_view>
#include <optional>
#include <cstdint>
#include <vector>
#include <string>
#include <array>
#include <map>

namespace protocol
{
    /* Protocol for messages: <SOH><length><STX><command><ETX> */ 
    inline constexpr size_t HEADER_SIZE = 4;     // Size of message header
    inline constexpr size_t MAX_COMMAND = 5000;  // Set capped size for commands (bytes)
    
    // Set minimum valid size for a single message (5 bytes)
    inline constexpr size_t MIN_FRAME = HEADER_SIZE + 1; 
    // Set capped size for a single message (5005 bytes)
    inline constexpr size_t MAX_FRAME = HEADER_SIZE + MAX_COMMAND + 1; 

    // Message delimiters.  
    inline constexpr char SOH = 0x01; // start of header
    inline constexpr char STX = 0x02; // start of text
    inline constexpr char ETX = 0x03; // end of text

    inline constexpr char HELO[]       = "HELO";
    inline constexpr char SERVERS[]    = "SERVERS";
    inline constexpr char KEEPALIVE[]  = "KEEPALIVE";
    inline constexpr char GETMSGS[]    = "GETMSGS";
    inline constexpr char SENDMSG[]    = "SENDMSG";
    inline constexpr char STATUSREQ[]  = "STATUSREQ";
    inline constexpr char STATUSRESP[] = "STATUSRESP";

    // Returns current timestamp
    std::string getTimestamp();

    // TODO: Add groupId validation
    int validateGroupId( const std::string &groupID );

    /* Creates a protocol compliant frame for the given message. */
    std::string createFrame( const std::string_view &message );

    /* Validates the given message if its protocol compliant for frames. */
    Frame parseMessage( const std::string_view &message );

    /* Finds the start of the next frame from a given buffer. */
    size_t findNextFrameStart( const std::string_view &message );

    /* Validates a given command and if it follows the protocol command formats. */
    std::optional< Payload > resolveCommand( const std::string_view &command );
};