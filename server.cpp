#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>   // For atoi()
#include <vector>
#include <algorithm> // For std::find_if, etc.
#include <sys/time.h>
#include <thread>

unsigned long timeUs() {
    struct timeval te; 
    gettimeofday(&te, NULL);
    return static_cast<unsigned long>(te.tv_sec) * 1000000ULL + te.tv_usec;
}

// A simple structure to store a client's address (IP/port)
struct ClientInfo {
    sockaddr_in addr;
};

// Compare two sockaddr_in structures by IP and port
bool sameClient(const sockaddr_in& a, const sockaddr_in& b) {
    return (a.sin_addr.s_addr == b.sin_addr.s_addr) &&
           (a.sin_port == b.sin_port) &&
           (a.sin_family == b.sin_family);
}

/**
 * Repeatedly recvfrom() until we've read 'size' bytes in total.
 * This is a naive approach; real-world UDP usage needs careful handling
 * of message boundaries, partial/lost packets, etc.
 */
ssize_t read_all(int sock, char* buffer, size_t size,
                 int e, int d,
                 sockaddr_in& clientAddr) // We'll populate/verify the client
{
    size_t total_read = 0;

    while (total_read < size) {
        unsigned int before = timeUs();

        socklen_t addrLen = sizeof(clientAddr);
        ssize_t bytes_read = recvfrom(sock,
                                      buffer + total_read,
                                      size - total_read,
                                      0,
                                      reinterpret_cast<sockaddr*>(&clientAddr),
                                      &addrLen);

        unsigned int interval = static_cast<unsigned int>(timeUs() - before);

        std::cout << "iteration " << e
                  << " decoder " << d
                  << ": bytes_read = " << bytes_read
                  << ", interval = " << interval << "us" << std::endl;

        if (bytes_read < 0) {
            perror("recvfrom error");
            return -1;
        } 
        // For UDP, if bytes_read == 0, it usually means an empty datagram (rare),
        // but not a "closed connection" as in TCP.
        if (bytes_read == 0) {
            break; 
        }
        total_read += bytes_read;
    }

    return total_read;
}

/**
 * Repeatedly sendto() until we've sent 'size' bytes in total.
 * This is also naive, because each sendto is one datagram.
 */
ssize_t send_all(int sock, const char* data, size_t size,
                 int e, int d,
                 const sockaddr_in& clientAddr)
{
    size_t total_sent = 0;

    while (total_sent < size) {
        unsigned int before = timeUs();

        ssize_t bytes_sent = sendto(sock,
                                    data + total_sent,
                                    size - total_sent,
                                    0,
                                    reinterpret_cast<const sockaddr*>(&clientAddr),
                                    sizeof(clientAddr));

        unsigned int interval = static_cast<unsigned int>(timeUs() - before);

        std::cout << "iteration " << e
                  << " decoder " << d
                  << ": bytes_sent = " << bytes_sent
                  << ", interval = " << interval << "us" << std::endl;

        if (bytes_sent < 0) {
            perror("sendto error");
            return -1;
        }
        total_sent += bytes_sent;
    }

    return total_sent;
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: server <data_size(KB)> <# of decoders> <# of clients> <port>" << std::endl;
        return -1;
    }

    // Extract command-line arguments
    int data_size   = std::atoi(argv[1]) * 1024; // Convert to bytes
    int iterations  = std::atoi(argv[2]) * 2;    // # decoders * 2
    int num_clients = std::atoi(argv[3]);        // How many clients to "register"
    int port        = std::atoi(argv[4]);

    // Allocate data buffers
    char* buffer = new char[data_size];
    char* data   = new char[data_size];

    // Create UDP socket
    int server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_fd < 0) {
        std::cerr << "Socket creation failed." << std::endl;
        delete[] buffer;
        delete[] data;
        return -1;
    }

    // Allow reuse of address/port
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "setsockopt(REUSEADDR) failed." << std::endl;
        close(server_fd);
        delete[] buffer;
        delete[] data;
        return -1;
    }

    // Bind the socket to the given port
    struct sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family      = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port        = htons(port);

    if (bind(server_fd, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        std::cerr << "Bind failed." << std::endl;
        close(server_fd);
        delete[] buffer;
        delete[] data;
        return -1;
    }

    std::cout << "UDP server listening on port " << port << "..." << std::endl;

    /**
     * In TCP, we would wait for `num_clients` connections via `accept()`.
     * For UDP, there's no connection handshake. We can "discover" clients
     * the first time they send data. We store their addresses in a vector.
     */
    std::vector<ClientInfo> clients;
    clients.reserve(num_clients);

    std::cout << "Waiting for " << num_clients << " distinct UDP clients..." << std::endl;

    while (static_cast<int>(clients.size()) < num_clients) {
        // Attempt to receive a small datagram just to identify a new client
        sockaddr_in clientAddr;
        socklen_t addrLen = sizeof(clientAddr);
        char tempBuf[16]; // small buffer
        ssize_t bytes = recvfrom(server_fd,
                                 tempBuf,
                                 sizeof(tempBuf),
                                 0,
                                 reinterpret_cast<sockaddr*>(&clientAddr),
                                 &addrLen);
        if (bytes < 0) {
            perror("recvfrom error");
            // You might want to continue listening despite an error
            continue;
        }

        // Check if this client is already known
        auto it = std::find_if(
            clients.begin(), clients.end(),
            [&](const ClientInfo& ci) {
                return sameClient(ci.addr, clientAddr);
            }
        );

        if (it == clients.end()) {
            // New client
            ClientInfo ci;
            ci.addr = clientAddr;
            clients.push_back(ci);
            std::cout << "Discovered new client ["
                      << clients.size() << "/" << num_clients
                      << "]" << std::endl;
        }
    }

    std::cout << "We have " << num_clients << " clients. Starting main loop." << std::endl;

    unsigned int sum_interval1 = 0;
    unsigned int sum_interval2 = 0;
    unsigned int sum_interval3 = 0;

    // Main loop
    for (int e = 0; e < 50; ++e) {
        sum_interval1 = 0;
        sum_interval2 = 0;
        sum_interval3 = 0;

        for (int i = 0; i < iterations; ++i) {
            // Prepare data
            std::memset(data, 'A' + (i % 26), data_size);

            unsigned int before1 = timeUs();

            // Send data to each known client
            for (auto& cli : clients) {
                ssize_t bytes_sent = send_all(server_fd, data, data_size, e, i, cli.addr);
                if (bytes_sent < 0) {
                    std::cerr << "Error in send_all()." << std::endl;
                }
            }

            // Read data from each known client
            for (auto& cli : clients) {
                // We'll call read_all once per client
                ssize_t bytes_read = read_all(server_fd, buffer, data_size, e, i, cli.addr);
                if (bytes_read < 0) {
                    std::cerr << "Error in read_all()." << std::endl;
                }
            }

            unsigned int interval1 = static_cast<unsigned int>(timeUs() - before1);
            sum_interval1 += interval1;

            std::cout << "iteration " << e
                      << " decoder " << i
                      << ": interval = " << interval1 << "us" << std::endl;
            std::cout << "==============================================================" << std::endl;
        }

        std::cout << "iteration " << e
                  << " Time = " << (sum_interval1 / 1000)
                  << " ms\n\n";
    }

    // Clean up
    close(server_fd);
    delete[] buffer;
    delete[] data;

    std::cout << "UDP server finished. Resources cleaned up." << std::endl;

    return 0;
}
