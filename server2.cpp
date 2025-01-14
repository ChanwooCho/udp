#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>

unsigned long timeUs() {
    struct timeval te; 
    gettimeofday(&te, NULL);
    return te.tv_sec * 1000000LL + te.tv_usec;
}

int main(int argc, char* argv[])
{
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0]
                  << " <datasize (KB)> <# of decoders> <port>\n";
        return 1;
    }

    // Parse command-line arguments
    int data_size_kb = std::atoi(argv[1]);
    int num_decoders = std::atoi(argv[2]);
    int port         = std::atoi(argv[3]);

    // Convert KB to bytes
    const size_t data_size = data_size_kb * 1024;

    // Create a UDP socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    // Bind the socket to the specified port (on all interfaces)
    sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // 0.0.0.0
    server_addr.sin_port        = htons(port);

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(sockfd);
        return 1;
    }

    std::cout << "UDP Server started on port " << port << "\n";

    // Prepare buffer
    char* buffer = new char[data_size];
    std::memset(buffer, 0, data_size);

    // We'll do a "handshake" to learn the client's address: 
    // The server blocks until it gets one packet from a client.
    sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    std::cout << "Waiting for initial message from client...\n";

    // Receive one packet from the client to learn its IP/port
    ssize_t initial_recv = recvfrom(
        sockfd,
        buffer,
        data_size,
        0,
        (struct sockaddr*)&client_addr,
        &client_addr_len
    );
    if (initial_recv < 0) {
        perror("recvfrom (initial handshake)");
        delete[] buffer;
        close(sockfd);
        return 1;
    }

    std::cout << "Received initial message from client. Starting main loop.\n";
    unsigned int before;
    unsigned int send_interval;
    unsigned int rece_interval;

    unsigned int before_all;
    unsigned int interval_all;

    // Main loop: 50 iterations of e, and (# of decoders * 2) iterations of i
    for (int e = 0; e < 50; ++e) {
        before_all = timeUs();
        for (int i = 0; i < num_decoders * 2; ++i) {
            // 1) Fill buffer
            std::memset(buffer, 'A' + (i % 26), data_size);

            // 2) Send data to the client
            //    We use sendto() with the client_addr we learned
            before = timeUs();
            ssize_t bytes_sent = sendto(
                sockfd,
                buffer,
                data_size,
                0,
                (struct sockaddr*)&client_addr,
                client_addr_len
            );
            send_interval = timeUs() - before;
            
            if (bytes_sent < 0) {
                perror("sendto");
                // Depending on needs, might break or continue
                break;
            }

            // 3) Receive data back from the client
            std::memset(buffer, 0, data_size);
            before = timeUs();
            ssize_t bytes_recv = recvfrom(
                sockfd,
                buffer,
                data_size,
                0,
                (struct sockaddr*)&client_addr,
                &client_addr_len
            );
            rece_interval = timeUs() - before;
            
            if (bytes_recv < 0) {
                perror("recvfrom");
                // Depending on needs, might break or continue
                break;
            }
            printf("iteration %d decoder %d, current_data = %c\n", e, i, buffer[data_size - 1]);
            printf("send_bytes = %zd, rece_bytes = %zd\n", bytes_sent, bytes_recv);
            printf("send_time = %dus, rece_time = %dus, sum_time = %dus\n", send_interval, rece_interval, send_interval + rece_interval);
            printf("========================\n");
        }
        interval_all = timeUs() - before_all;
        printf("interval_all = %dus\n", interval_all);
    }

    std::cout << "Server finished sending/receiving.\n";

    // Clean up
    delete[] buffer;
    close(sockfd);

    return 0;
}
