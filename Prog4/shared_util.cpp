#include "shared_util.h"

#include <charconv>
#include <chrono>

namespace util
{

    uint16_t validatePortNum( const std::string_view &portText )
    {
        uint16_t port = 0;
        const auto [end, error] = std::from_chars(
                    portText.data(), portText.data() + portText.size(), port);

        if( error != std::errc() || end != portText.data() + portText.size() )
        {
            return -1;
        }

        if( port < 4000 || port > 4200 )
        {
            return -1;
        }

        return port;
    }

    int64_t convertTimestamp( const std::string_view &timestampText )
    {
        int64_t timestamp = 0;
        const auto [end, error] = std::from_chars(
                timestampText.data(), timestampText.data() + timestampText.size(), timestamp);

        if( error != std::errc() || end != timestampText.data() + timestampText.size() )
        {
            return -1;
        }

        return timestamp;
    }

    int64_t getTimestamp()
    {
        const auto timestamp = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>( timestamp.time_since_epoch()).count();
    }
}