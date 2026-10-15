
#pragma once

#include <string>

/* Waits for and writes the servers reply into a buffer. */
bool receiveFrame(int sock, std::string& receiveBuffer, std::string& response);

