#include <iostream>
#include <string>
#include <cstdint>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>
#include "tsamclient.h"
#include <ctime>
#include <iomanip>
#include <sstream>

std::string createFrame(const std::string& command)
{
    uint16_t length =   command.size() + 5;
    uint16_t networkLength = htons(length);
    std::string frame;
    frame.push_back('\x01'); // SOH

    frame.append(reinterpret_cast<const char*>(&networkLength), sizeof(networkLength));

    frame.push_back('\x02'); // STX
    frame += command;
    frame.push_back('\x03'); // ETX

    return frame;
}


std::string decodeFrame(const std::string& frame)
{
    // A frame must contain at least 5 bytes
    if (frame.size() < 5)
    {
        return "Invalid frame";
    }

    // Check framing characters
    if (frame[0] != '\x01' ||
        frame[3] != '\x02' ||
        frame.back() != '\x03')
    {
        return "Invalid frame";
    }

    // Read the 2-byte length field
    uint16_t length =
        (static_cast<uint8_t>(frame[1]) << 8) |
        static_cast<uint8_t>(frame[2]);

    if (length != frame.size())
    {
        return "Invalid frame";
    }

    // Extract text between STX and ETX
    return frame.substr(4, frame.size() - 5);
}


bool receiveFrame(int sock, std::string& receiveBuffer, std::string& response)
{
    char temp[1024];

    while (true)
    {
        // Check whether we have enough bytes for a header
        if (receiveBuffer.size() >= 4)
        {
            if (receiveBuffer[0] != '\x01' ||
                receiveBuffer[3] != '\x02')
            {
                std::cerr << "Invalid response header\n";
                return false;
            }

            // Extract frame length
            uint16_t length =
                (static_cast<uint8_t>(receiveBuffer[1]) << 8) |
                static_cast<uint8_t>(receiveBuffer[2]);

            if (length < 5 || length > 5005)
            {
                std::cerr << "Invalid response length\n";
                return false;
            }

            // Check if the whole frame has arrived
            if (receiveBuffer.size() >= length)
            {
                std::string frame =
                    receiveBuffer.substr(0, length);

                receiveBuffer.erase(0, length);

                if (frame.back() != '\x03')
                {
                    std::cerr << "Invalid response ending\n";
                    return false;
                }

                response = frame.substr(4, length - 5);
                return true;
            }
        }

        // Receive more data from the server
        ssize_t n = recv(sock, temp, sizeof(temp), 0);

        if (n < 0 && errno == EINTR)
        {
            continue;
        }

        if (n < 0)
        {
            perror("Error: recv");
            return false;
        }

        if (n == 0)
        {
            std::cerr << "Server disconnected\n";
            return false;
        }

        receiveBuffer.append(
            temp, static_cast<size_t>(n)
        );
    }
}

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


int main(int argc, char* argv[])
{
    if (argc != 3){
        std:: cerr << "Usage: ./tsamclient <IP> <port>\n"; 
        return 1;
    }
    std::string ip = argv[1];
    int port = 0;

    try
    {
        port = std::stoi(argv[2]);
    }
    catch(...)
    {
        std::cerr << "invalid port\n";
        return 1;
    }
    
    if (port < 1 || port > 65535)
    {
        std::cerr << "Port must be between 1 and 65535\n";
        return 1;
    }


    int sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0)
    {
        perror("Error: Socket ");
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr) != 1)
    {
        std::cerr << "Error: Invalid IP address\n";
        close(sock);
        return 1;
    }

    if (connect(sock, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0)
    {
        perror("Error: Connect ");
        close(sock);
        return 1;
    }

    std::cout << "Connected to server!\n";
    std:: string command;
    std::string receiveBuffer;

    while(true){
        std::cout <<  "\n" << std::flush;

        if (!std::getline(std::cin, command))
        {
            break;
        }
        if (command == "Quit" || command == "QUIT" || command == "quit")
        {
            break;
        }

        if (command.size() > 5000)
        {
            std::cerr << "Error: Command exceeds 5000 bytes\n";
            continue;
        }
        std::string frame = createFrame(command);
        std::cout << "[" << getTimestamp()<< "] Sending: " << command << "\n";

       
        size_t sent = 0;

        while (sent < frame.size())
        {
            ssize_t n = send( sock, frame.data() + sent, frame.size() - sent, 0 );
            if (n < 0 && errno == EINTR)
            {
                continue;
            }

            if (n <= 0)
            {
                perror("Error: Send");
                close(sock);
                return 1;
            }

            sent += static_cast<size_t>(n);
        }
        std::string response;

        if (!receiveFrame(sock, receiveBuffer, response))
        {
            break;
        }
        std::cout << "[" << getTimestamp()<< "] Server responded: " << response << "\n";
    }
    close(sock);
    return 0;



}