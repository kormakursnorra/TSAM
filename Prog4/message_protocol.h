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

    // Command char* constants
    
    /*
     * Command Command Format: HELO,<FROM_GROUP_ID>.
     * Reply with the SERVERS response (below). This must be the first command each end 
     * sends on a connection: send it directly after connect() and accept().
     */
    inline constexpr char HELO[]       = "HELO";

    /*
     * Command Command Format: SERVERS,<server list>.
     * Response to HELO: a semicolon-separated list of the directly connected servers of the responding server. 
     * Each server is specified as <GROUP_ID>,<IP>,<PORT> with the host IP and the port they accept connections on.
     * Example: SERVERS,G_banana,130.208.246.98,4001;G_pelican,130.208.246.98,4002;
     */
    inline constexpr char SERVERS[]    = "SERVERS";

    /*
     * Command Format: KEEPALIVE,<No. of messages>.
     * Periodic message to 1-hop connected servers, indicating the no. of messages the server sending the KEEPALIVE 
     * message has waiting for the server at the other end. Do not send more than once a minute.
     */
    inline constexpr char KEEPALIVE[]  = "KEEPALIVE";

    /*
     * Command Format: GETMSGS,<GROUP_ID>.
     * Get messages for the specified group in the form of SENDMSG commands.
     */
    inline constexpr char GETMSG[]     = "GETMSG";

    /*
     * Command Format: SENDMSG,<FROM_GROUP_ID>,<TO_GROUP_ID>,<TIMESTAMP>,<message content>.
     * Send a message to another group. <TIMESTAMP> is the time when the message was first created as seconds since 
     * the Unix epoch (UTC). 
     * Example: SENDMSG,G_banana,G_pelican,1792915407,Hello, world.
     */
    inline constexpr char SENDMSG[]    = "SENDMSG";

    /*
     * Command Format: STATUSREQ.
     * Reply with STATUSRESP as below.
     */
    inline constexpr char STATUSREQ[]  = "STATUSREQ";

    /*
     * Command Format: STATUSRESP,<name1>:<count1>,<name2>:<count2>,...
     * Response to STATUSREQ: the servers you are holding messages for and how many, listing only servers with at 
     * least one stored message. If you are holding no messages at all, the response is STATUSRESP,EMPTY.
     * Example: STATUSRESP,G_tapir:20,I_oracle:2.
     */
    inline constexpr char STATUSRESP[] = "STATUSRESP";

    

    // Structs that represent/wrap the command types/data. 
    struct SendMsg   { std::string sender; std::string recipient; std::string contents; };
    struct GetMsg    { std::string username; };
    struct StatusReq {};

    using Payload = std::variant< SendMsg, GetMsg, StatusReq >;
    
    /* Enum values represent the state of a given message:
     * Ok         -> Valid frame parsed. 
     * Incomplete -> Missing data.
     * Malformed  -> Not protocol formatted. 
     */
    enum class FrameStatus { Ok, Incomplete, Malformed };

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
     * Validates the given message if its protocol compliant for frames.
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