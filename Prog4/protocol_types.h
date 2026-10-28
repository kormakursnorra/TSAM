#pragma once

#include <cstdint>
#include <variant>
#include <string>
#include <vector>
#include <deque>
#include <map>


namespace protocol
{
    // Generic Message struct
    struct Message 
    { 
        std::string sender    = ""; 
        std::string recipient = ""; 
        int64_t timeStamp     = -1; 
        std::string content   = "";
    };

    // Servers and for and how many holding messages 
    using ServerCount = std::map< std::string, uint8_t >;

    // GroupIds and their messages
    using Mailboxes = std::map< std::string, std::deque< Message > >;

    /* 
    * Wrapper for a connected server instance: <GROUP_ID>,<IP>,<PORT>. 
    * SERVERS entry.
    */
    struct ServerInfo 
    { 
        std::string groupId = ""; 
        std::string ipAddr  = ""; 
        uint16_t listenPort = -1;
        
        ServerInfo( const std::string &id, const std::string &ip, uint32_t port ) 
        : groupId( id ), ipAddr( ip ), listenPort( port ) {} 
    };

    // Structs that represent/wrap the command types/data. 

    /* 
     * Command Format: HELO,<FROM_GROUP_ID>.
     * Reply with the SERVERS response (below). This must be the first command each end 
     * sends on a connection: send it directly after connect() and accept().
     */
    struct Helo { std::string fromGroupId; };

    /*
     * Command Format: SERVERS,<server list>.
     * Response to HELO: a semicolon-separated list of the directly connected servers of the responding server. 
     * First entry is the list is always our own server. Each server is specified as <GROUP_ID>,<IP>,<PORT> 
     * with the host IP and the port they accept connections on.
     * Example: SERVERS,G_banana,130.208.246.98,4001;G_pelican,130.208.246.98,4002;
     */
    struct Servers { std::vector< struct ServerInfo > serverList; };

    /*
     * Command Format: KEEPALIVE,<No. of messages>.
     * Periodic message to 1-hop connected servers, indicating the no. of messages the server sending the KEEPALIVE 
     * message has waiting for the server at the other end. Do not send more than once a minute.
     */
    struct KeepAlive { uint8_t numMsg; };

    /*
     * Command Format: GETMSGS,<GROUP_ID>.
     * Get messages for the specified group in the form of SENDMSG commands.
     */
    struct GetMsg { std::string groupId; };

    /*
     * Command Format: SENDMSG,<FROM_GROUP_ID>,<TO_GROUP_ID>,<TIMESTAMP>,<message content>.
     * Send a message to another group. <TIMESTAMP> is the time when the message was first created as seconds since 
     * the Unix epoch (UTC). 
     * Example: SENDMSG,G_banana,G_pelican,1792915407,Hello, world.
     */
    struct SendMsg { std::string fromGroupId; std::string toGroupId; int64_t timeStamp; std::string msgContents; };

    /*
     * Command Format: STATUSREQ.
     * Reply with STATUSRESP as below.
     */
    struct StatusReq {};

    /*
     * Command Format: STATUSRESP,<name1>:<count1>,<name2>:<count2>,...
     * Response to STATUSREQ: the servers you are holding messages for and how many, listing only servers with at 
     * least one stored message. If you are holding no messages at all, the response is STATUSRESP,EMPTY.
     * Example: STATUSRESP,G_tapir:20,I_oracle:2.
     */
    struct StatusResp { ServerCount serverMsgCount; };

    // Command type structs can be passed in as Payload to functions
    using Payload = std::variant< Helo, Servers, KeepAlive, GetMsg, SendMsg, StatusReq, StatusResp >;

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

};