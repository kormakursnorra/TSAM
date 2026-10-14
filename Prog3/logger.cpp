#include "logger.h"

#include <exception>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdint>
#include <string>
#include <fstream>

std::ofstream logFile;


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

bool logInit( const std::string &path )
{
    logFile.open( path.c_str(), std::ios::out | std::ios::app );
    logFile.exceptions( std::ios::failbit );
    return static_cast< bool >( logFile );    
}


int logToFile( const std::string &logMsg )
{
    std::string logLine  = "[ " + getTimestamp() + " ] " + logMsg;
    if( logFile )
    {
        try
        {
            logFile << logLine << '\n' << std::flush;
        }
        catch( const std::exception & ex )
        {
            std::cerr << "Could not write to log file: " << ex.what() << std::endl;;
        }
    }
    
    std::cerr << logLine << '\n'; 
}

void logEnd()
{
    try 
    {
        logFile.close();
        
    } catch ( const std::exception & ex ) 
    {
        std::cerr << "Failed to close log file: " << ex.what() << std::endl;    
    }
    
}
