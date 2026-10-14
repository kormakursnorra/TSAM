#include "logger.h"

#include <iomanip>
#include <sstream>
#include <cstdint>

std::string getTimestamp()
{
    std::time_t now = std::time(nullptr);

    std::tm localTime{};

    localtime_r(&now, &localTime);

    std::ostringstream oss;

    oss << std::put_time(
        &localTime, "%Y-%m-%d %H:%M:%S"
    );

    return oss.str();
}

int logToFile()
{
    
}