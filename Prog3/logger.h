#pragma once

#include <string>

std::string getTimestamp();

bool logInit( const std::string &path );

int logToFile( const std::string &logMsg );

void logEnd();