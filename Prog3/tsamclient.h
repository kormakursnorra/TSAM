
#pragma once

#include <string>

std::string decodeFrame(const std::string& frame);

bool receiveFrame(int sock,
                  std::string& receiveBuffer,
                  std::string& response);

std::string getTimestamp();
