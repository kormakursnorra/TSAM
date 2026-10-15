#pragma once

#include <utility>
#include <string>

namespace logger
{

std::string getTimestamp();

enum class LogLevel { INFO, WARNING, ERROR };

struct Log 
{
    LogLevel logLevel = LogLevel::INFO;
    std::string logMsg;
    std::string logTime;
    
    Log() = default;

    Log( LogLevel level, const std::string &msg )
        : logLevel( level ), logMsg( std::string( msg ) ), logTime( getTimestamp() ) {}

    std::string format() const;
};

bool logInit( const std::string &path, bool logDate );

void logToFile( const struct Log &log );

void closeLog();

};
