#pragma once

#include <utility>
#include <string>

namespace logger
{

/* 
 * Returns string representing the current time (Year-Month-Day Hour:Minute:Second).
 */
std::string getTimestamp();

/* 
 * Enum for classifying different levels of log events 
 */
enum class LogLevel { INFO, WARNING, ERROR };

/* 
 * Wrapper for basic logging information (level of log event, log message, time of log). 
 */
struct Log 
{
    LogLevel logLevel = LogLevel::INFO;
    std::string logMsg;
    std::string logTime;
    
    Log() = default;

    Log( LogLevel level, const std::string &msg )
        : logLevel( level ), logMsg( std::string( msg ) ), logTime( getTimestamp() ) {}

    /* Formats the logging information into a readable string */
    std::string format() const;
};

/* 
 * Opens/creates a file to log events during server operations. 
 * Returns true on success and false on error.  
 */
bool logInit( const std::string &name, bool logDate );

/*
 * Writes to the log file and to stdcerr/stdcout the formatted information 
 * from a given Log instance. 
 */
void logToFile( const struct Log &log );

/*
 * Closes the log file. 
 */
void closeLog();

};
