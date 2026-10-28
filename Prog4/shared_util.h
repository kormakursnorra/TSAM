#pragma once 

#include <string_view>
#include <cstdint>

namespace util
{
    // Tries to convert and return given port number string_view
    uint16_t validatePortNum( const std::string_view &portText ); 

    // Returns current timestamp in seconds
    int64_t getTimestamp();

    // Converts timestamp string to int64_t 
    int64_t convertTimestamp( const std::string_view &timestampText );
};