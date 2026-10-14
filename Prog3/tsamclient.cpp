#include "message_protocol.h"

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

bool receiveFrame(int sock, std::string& receiveBuffer, std::string& response)
{
    char temp[ protocol::MAX_FRAME ];

    while (true)
    {
        // Receive more data from the server
        ssize_t bytesReceived = recv(sock, temp, sizeof(temp), 0);

        if (bytesReceived < 0 && errno == EINTR)
        {
            continue;
        }

        if (bytesReceived < 0)
        {
            perror("Error: recv");
            return false;
        }

        if (bytesReceived == 0)
        {
            std::cerr << "Server disconnected\n";
            return false;
        }

        receiveBuffer.append(
            temp, static_cast< size_t >( bytesReceived )
        );
        
        auto frame = protocol::parseMessage( receiveBuffer );
        
        if( frame.status == protocol::FrameStatus::Incomplete )
        {
            return false;
        }
        
        if ( frame.status == protocol::FrameStatus::Malformed )
        {
            size_t skip = protocol::findNextFrameStart( receiveBuffer );
            std::cerr << "Malformed input, discarding " << skip << " bytes\n";
            receiveBuffer.erase(0, skip);
            continue;
        }

        
        response = frame.body;    
        receiveBuffer.erase( 0, frame.frameLen );
        return true;
    }
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

    while(true) {
        std::cout <<  "\n" << std::flush;

        if (!std::getline(std::cin, command))
        {
            break;
        }

        if( command.empty() )
        {
            continue;
        }

        if (command.size() > 5000)
        {
            std::cerr << "Error: Command exceeds 5000 bytes\n";
            continue;
        }
        std::string frame = protocol::createFrame( command );
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

        if ( !receiveFrame(sock, receiveBuffer, response) )
        {
            break;
        }
        
        std::cout << "[" << getTimestamp()<< "] Server responded: " << response << "\n";
    }
    close(sock);
    return 0;



}