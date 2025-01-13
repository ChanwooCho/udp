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
                  << " <datasize (KB)> <# of decoders> <port>\n";
        return 1;
    }

    // Parse command-line arguments
    int data_size_kb = std::atoi(argv[1]);
    int num_decoders = std::atoi(argv[2]);
    int port = std::atoi(argv[3]);

    // Convert KB to bytes
    const size_t data_size = data_size_kb * 1024;

    // Create a UDP socket
    int sockfd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    // Bind the socket to the given port on all interfaces
    sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(port);

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(sockfd);
        return 1;
    }

    // Buffer for sending/receiving data
    char* buffer = new char[data_size];

    std::cout << "UDP Server started on port " << port << "\n";
    std::cout << "Waiting for an initial client message to learn their address...\n";

    // First, receive one message to learn the client's address (handshake)
    sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    ssize_t initial_recv = recvfrom(
        sockfd, buffer, data_size, 0,
        (struct sockaddr*)&client_addr, &client_addr_len
    );
    if (initial_recv < 0) {
        perror("recvfrom (initial)");
        delete[] buffer;
        close(sockfd);
        return 1;
    }
    std::cout << "Received first packet from client. Address learned.\n";

    // Optionally, "connect" this UDP socket to the client so we can use send/recv
    // instead of sendto/recvfrom. This is optional but can simplify the code.
    if (connect(sockfd, (struct sockaddr*)&client_addr, client_addr_len) < 0) {
        perror("connect");
        delete[] buffer;
        close(sockfd);
        return 1;
    }

    // Now the main loop:
    //   for e in [0..49]:
    //     for i in [0..num_decoders*2 - 1]:
    //       1) fill buffer
    //       2) send data
    //       3) read data back
    for (int e = 0; e < 50; ++e) {
        for (int i = 0; i < num_decoders * 2; ++i) {
            // Fill buffer with repeated character
            std::memset(buffer, 'A' + (i % 26), data_size);

            // Send the data (UDP)
            ssize_t bytes_sent = send(sockfd, buffer, data_size, 0);
            if (bytes_sent < 0) {
                perror("send");
                // You may choose to continue or break, depending on your needs
                break;
            }

            // Receive data back (UDP)
            ssize_t bytes_recv = recv(sockfd, buffer, data_size, 0);
            if (bytes_recv < 0) {
                perror("recv");
                // You may choose to continue or break, depending on your needs
                break;
            }

            // (Optional) Do something with the received data...
        }
    }

    std::cout << "Server finished sending/receiving.\n";

    // Cleanup
    delete[] buffer;
    close(sockfd);
    return 0;
}
