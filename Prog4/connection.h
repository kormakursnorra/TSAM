#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <poll.h>
#include <map>

namespace connection
{
    // Connection kind can be unknown, our client, or peer server.
    enum class ConnectionKind { Unknown, OwnClient, Peer };

    /*
    * Wrapper for a connected clients data.  
    */
    struct Connection
    {
        int32_t socket; std::string remoteIp; uint16_t remotePort; // the socket's actual other end, for log_event

        ConnectionKind kind = ConnectionKind::Unknown;  // connection kind (unknown, our client, or peer server)
        std::string peerGroupId = "";                       // HELO name if exchanged
        uint16_t peerListenPort = 0;                       // peer's listening port, if known  
        bool heloExchanged  = false;                    // if HELO has been exchanged
        uint64_t lastSeen   = 0;                       // when was client last seen (seconds)
        std::string inbox   = "";                       // bytes received but not yet parsed
        std::string outbox  = "";                       // outbound buffer

        Connection( int sock, const std::string &ip, uint16_t port ) 
        : socket( sock ), remoteIp( ip ), remotePort( port ){} 
    };

    // Lookup table for per Connection information
    using ConnectionLookup = std::map< int, Connection >;

    // List of all polled fds
    using PolledFds = std::vector< struct pollfd >;

};