#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main(int argc, char* argv[])
{
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

    // Extract IP and port from "ip_address:port"
    auto colon_pos = ip_port.find(':');
    if (colon_pos == std::string::npos) {
        std::cerr << "[Client] Error: invalid ip_address:port format.\n";
        return 1;
    }
    std::string ip_str = ip_port.substr(0, colon_pos);
    int port           = std::atoi(ip_port.substr(colon_pos + 1).c_str());

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

    // Allocate buffer
    char* buffer = new char[data_size];

    std::cout << "[Client] UDP client sending to " << ip_str << ":" << port << "\n";

    // 1) Send an initial packet to let the server learn our address
    std::memset(buffer, 'C', data_size);
    ssize_t handshake_sent = sendto(
        sockfd,
        buffer,
        data_size,
        0,
        (struct sockaddr*)&server_addr,
        sizeof(server_addr)
    );
    if (handshake_sent < 0) {
        perror("sendto (handshake)");
        delete[] buffer;
        close(sockfd);
        return 1;
    }
    std::cout << "[Client] Sent handshake.\n";

    // 2) Main loop: 
    //    for e in [0..49], for i in [0..(num_decoders*2 - 1)]:
    //      - fill buffer
    //      - recvfrom() from the server
    //      - sendto() back to the server
    sockaddr_in from_addr;
    socklen_t from_addr_len = sizeof(from_addr);

    for (int e = 0; e < 50; ++e) {
        for (int i = 0; i < num_decoders * 2; ++i) {
            // Fill buffer
            std::memset(buffer, 'A' + (i % 26), data_size);

            // Receive data from server
            std::memset(buffer, 0, data_size);
            ssize_t bytes_recv = recvfrom(
                sockfd,
                buffer,
                data_size,
                0,
                (struct sockaddr*)&from_addr,
                &from_addr_len
            );
            if (bytes_recv < 0) {
                perror("recvfrom");
                // Depending on requirements, you might break or continue
                break;
            }

            // (Optional) process the data from the server here...
            // For demonstration, we just send the same data back.

            // Send data back to the server
            // (Using the same server_addr from the beginning is also possible,
            //  but this demonstrates using the from_addr we just received from.)
            ssize_t bytes_sent = sendto(
                sockfd,
                buffer,
                data_size,
                0,
                (struct sockaddr*)&from_addr,
                from_addr_len
            );
            if (bytes_sent < 0) {
                perror("sendto");
                // Depending on requirements, you might break or continue
                break;
            }
        }
    }

    std::cout << "[Client] Finished sending/receiving.\n";

    // Cleanup
    delete[] buffer;
    close(sockfd);
    return 0;
}
