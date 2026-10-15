#include "logger.h"

#include <exception>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <string>
#include <ctime>

namespace logger 
{

static std::ofstream logFile;

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

inline const char *levelName( LogLevel level )
{
    switch( level )
    {
        case LogLevel::INFO:    return "[ INFO ] ";
        case LogLevel::WARNING: return "[ WARNING ] ";
        case LogLevel::ERROR:   return "[ ERROR ] ";
    }
    return "[ ? ] ";
};


std::string Log::format() const
{
    return "[ " + logTime + " ] " + levelName( logLevel ) + logMsg;
}



bool logInit( const std::string &name )
{
    std::string path = name + getTimestamp() + ".log";
    logFile.open( path.c_str(), std::ios::out | std::ios::app );
    if( logFile.is_open() )
    {
        logFile.exceptions( std::ios::failbit );
    }
    return static_cast< bool >( logFile );    
}


void logToFile( const struct Log &log )
{
    if( logFile )
    {
        try
        {
            logFile << log.format() << std::endl << std::flush;
        }
        catch( const std::exception & ex )
        {
            std::cerr << "Could not write to log file: " << ex.what() << std::endl;
        }
    }

    if( log.logLevel == LogLevel::ERROR )
    {
        std::cerr << log.format() << std::endl;
    }
    else {
        std::cout << log.format() << std::endl;    
    }
}

void closeLog()
{
    try 
    {
        logFile.close();
        
    } catch ( const std::exception & ex ) 
    {
        std::cerr << "Failed to close log file: " << ex.what() << std::endl;    
    }
    
}

};