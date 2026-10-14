
#pragma once

#include <string>


std::string getTimestamp();

bool receiveFrame(int sock, std::string& receiveBuffer, std::string& response);

