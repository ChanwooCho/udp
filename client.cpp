#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0]
                  << " <datasize (KB)> <# of decoders> <ip_address:port>\n";
        return 1;
    }

    // Parse command-line arguments
    int data_size_kb = std::atoi(argv[1]);
    int num_decoders = std::atoi(argv[2]);
    std::string ip_port = argv[3];

    // Convert KB to bytes
    const size_t data_size = data_size_kb * 1024;

    // Extract IP and port from the third argument "<ip_address:port>"
    auto colon_pos = ip_port.find(':');
    if (colon_pos == std::string::npos) {
        std::cerr << "Invalid ip_address:port format.\n";
        return 1;
    }
    std::string ip_str   = ip_port.substr(0, colon_pos);
    int port             = std::atoi(ip_port.substr(colon_pos + 1).c_str());

    // Create a UDP socket
    int sockfd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    // Server address
    sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(port);

    if (inet_pton(AF_INET, ip_str.c_str(), &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sockfd);
        return 1;
    }

    // Optionally, we can call connect so we can just use send/recv
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sockfd);
        return 1;
    }

    // Allocate buffer for sending/receiving
    char* buffer = new char[data_size];

    std::cout << "UDP Client connecting to " << ip_str << ":" << port << "\n";

    // Send an initial packet to let the server learn our address
    std::memset(buffer, 'C', data_size); // Fill with some placeholder data
    ssize_t initial_sent = send(sockfd, buffer, data_size, 0);
    if (initial_sent < 0) {
        perror("send (initial)");
        delete[] buffer;
        close(sockfd);
        return 1;
    }

    // Main loop
    for (int e = 0; e < 50; ++e) {
        for (int i = 0; i < num_decoders * 2; ++i) {
            // Fill buffer with repeated character
            std::memset(buffer, 'A' + (i % 26), data_size);

            // Receive data from server
            ssize_t bytes_recv = recv(sockfd, buffer, data_size, 0);
            if (bytes_recv < 0) {
                perror("recv");
                // Decide whether to exit or continue
                break;
            }

            // (Optionally) process the data...

            // Send data back to the server
            ssize_t bytes_sent = send(sockfd, buffer, data_size, 0);
            if (bytes_sent < 0) {
                perror("send");
                // Decide whether to exit or continue
                break;
            }
        }
    }

    std::cout << "Client finished sending/receiving.\n";

    // Cleanup
    delete[] buffer;
    close(sockfd);
    return 0;
}
