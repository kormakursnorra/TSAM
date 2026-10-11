#include <string_view>
#include <optional>
#include <variant>
#include <vector>
#include <string>
#include <array>

/*
Message delimeters. 
    SOH (Start-of-header): 0x01
    STX (Start-of-text): 0x02
    ETX (End-of-text): 0x03
*/
enum MessageDelimiter { SOH = 0x01, STX = 0x02, ETX = 0x03 };

enum class MessageCommand { SendMsg, GetMsg, StatusReq };

inline constexpr std::array<std::string_view, 3> msgCommandNames = {
    "SENDMSG", "GETMSG", "STATUSREQ"
};

constexpr std::string_view toString( MessageCommand c ) 
{
    return msgCommandNames[ static_cast< std::size_t >( c ) ];
}

constexpr std::optional< MessageCommand > parseCommand( std::string_view s ) 
{
    for (std::size_t i = 0; i < msgCommandNames.size(); ++i)
    {
        if (msgCommandNames[i] == s)
            return static_cast< MessageCommand >( i );
    }
    return std::nullopt;
}

struct SendMsg   { std::string sender; std::string recipient; std::string body; };
struct GetMsg    { std::string username; };
struct StatusReq {};

using Content = std::variant< SendMsg, GetMsg, StatusReq >;

struct Message
{
    int soh = SOH;
    int length = -1;
    int stx = SOH;
    Content content;
    int etx = ETX;
    
};

/*
This class provides functionality to parse and verify messages from clients. It processes
raw, data packets into defined Message structs.
*/
class MessageHandler
{
public:

    MessageHandler();     // Constructor

    /* 
    Parse and unpack buffer (data packet). If size of buffer > 5000 bytes, it may be discarded. 
    */
    int parseMessage( char* buffer );
    
    int sendMessage( );

    int getMessage( );

    int statusRequest( );


    
    ~MessageHandler(){}                 // Default destructor for class
    
    


};