#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <netinet/in.h>
#include <sys/time.h>

// Helper function to get current timestamp in microseconds
unsigned long timeUs() {
    struct timeval te; 
    gettimeofday(&te, NULL);
    return static_cast<unsigned long>(te.tv_sec) * 1000000ULL + te.tv_usec;
}

unsigned int min_latency = 999999999; // Arbitrary large initial value

/**
 * Repeatedly recvfrom() until we've read 'size' bytes in total.
 * This is naive because UDP is message-oriented and
 * the data can arrive in multiple datagrams (or be lost).
 */
ssize_t read_all(int sock, char* buffer, size_t size, int e, int d,
                 sockaddr_in& serverAddr)
{
    size_t total_read = 0;
    bool is_first = true;

    while (total_read < size) {
        unsigned long before = timeUs();

        socklen_t addrLen = sizeof(serverAddr);
        // We'll receive from the server's address
        ssize_t bytes_read = recvfrom(sock,
                                      buffer + total_read,
                                      size - total_read,
                                      0,
                                      reinterpret_cast<struct sockaddr*>(&serverAddr),
                                      &addrLen);

        unsigned int interval = static_cast<unsigned int>(timeUs() - before);

        if (bytes_read < 0) {
            perror("recvfrom error");
            return -1;
        } 
        // In UDP, bytes_read == 0 can occur for an empty datagram,
        // but it's not a "closed" connection like in TCP.
        if (bytes_read == 0) {
            break;
        }

        // Track minimal latency for the first read in each loop
        if (is_first && interval < min_latency) {
            min_latency = interval;
        }
        is_first = false;

        std::cout << "iteration " << e
                  << " decoder " << d
                  << ": bytes_read = " << bytes_read
                  << ", interval = " << interval << "us" << std::endl;

        total_read += bytes_read;
    }

    return total_read;
}

/**
 * Repeatedly sendto() until we've sent 'size' bytes in total.
 * This is also naive for UDP—each sendto is one datagram, and
 * we’re ignoring potential packet fragmentation.
 */
ssize_t send_all(int sock, const char* data, size_t size, int e, int d,
                 const sockaddr_in& serverAddr)
{
    size_t total_sent = 0;

    while (total_sent < size) {
        unsigned long before = timeUs();

        ssize_t bytes_sent = sendto(sock,
                                    data + total_sent,
                                    size - total_sent,
                                    0,
                                    reinterpret_cast<const struct sockaddr*>(&serverAddr),
                                    sizeof(serverAddr));

        unsigned int interval = static_cast<unsigned int>(timeUs() - before);

        if (bytes_sent < 0) {
            perror("sendto error");
            return -1;
        }

        std::cout << "iteration " << e
                  << " decoder " << d
                  << ": bytes_send = " << bytes_sent
                  << ", interval = " << interval << "us" << std::endl;

        total_sent += bytes_sent;
    }

    return total_sent;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: client <data_size(KB)> <# of decoders> <ip_address:port>" << std::endl;
        return -1;
    }

    // Parse data_size and iterations
    int data_size  = std::atoi(argv[1]) * 1024; // Bytes
    int iterations = std::atoi(argv[2]) * 2;   // # decoders * 2

    // Split the IP address and port
    std::string input(argv[3]);
    std::size_t colon_pos = input.find(':');
    if (colon_pos == std::string::npos) {
        std::cerr << "Invalid argument format. Use: <ip address:port>" << std::endl;
        return -1;
    }
    std::string ip_address = input.substr(0, colon_pos);
    int port = std::atoi(input.substr(colon_pos + 1).c_str());

    // Allocate buffers
    char* buffer = new char[data_size];
    char* data   = new char[data_size];
    std::memset(buffer, 0, data_size);
    std::memset(data,   0, data_size);

    // Create a UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "Socket creation error" << std::endl;
        delete[] buffer;
        delete[] data;
        return -1;
    }

    // Fill in server address
    sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port   = htons(port);

    // Convert IP address from text to binary
    if (inet_pton(AF_INET, ip_address.c_str(), &serverAddr.sin_addr) <= 0) {
        std::cerr << "Invalid address/Address not supported" << std::endl;
        close(sock);
        delete[] buffer;
        delete[] data;
        return -1;
    }

    std::cout << "UDP client set to communicate with server at "
              << ip_address << ":" << port << std::endl;

    // Main loop
    for (int e = 0; e < 50; ++e) {
        for (int i = 0; i < iterations; ++i) {
            // Prepare data for sending
            std::memset(data, 'A' + (i % 26), data_size);

            unsigned long before1 = timeUs();

            // 1) Receive data_size bytes *from* the server
            //    (In some protocols, the server might send first, or we might
            //     send first. This is just matching your TCP logic.)
            ssize_t bytes_received = recvfrom(sock,
                                      buffer + total_read,
                                      size - total_read,
                                      0,
                                      reinterpret_cast<struct sockaddr*>(&serverAddr),
                                      &addrLen);
            if (bytes_received < 0) {
                std::cerr << "Error receiving from server." << std::endl;
                break;
            }

            // 2) Send data_size bytes *back* to the server
            ssize_t bytes_sent = sendto(sock,
                                    data + total_sent,
                                    size - total_sent,
                                    0,
                                    reinterpret_cast<const struct sockaddr*>(&serverAddr),
                                    sizeof(serverAddr));

            unsigned long interval1 = timeUs() - before1;
            std::cout << "iteration " << e
                      << " decoder " << i
                      << ": interval = " << interval1 << "us\n";
            std::cout << "==============================================================" << std::endl;
        }
        // You can print out stats (like min_latency) here if you like
        // std::cout << "Minimum latency so far = " << min_latency << "us" << std::endl;
    }

    // Cleanup
    close(sock);
    delete[] buffer;
    delete[] data;

    std::cout << "UDP client finished. Exiting." << std::endl;

    return 0;
}
