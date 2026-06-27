#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

int main() {
    // 1. Create a client socket (IPv4, TCP)
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        std::cerr << "Failed to create socket.\n";
        return 1;
    }

    // 2. Define the target server address
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080); // Target port
    
    // Convert IPv4 text address to binary format
    if (inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr) <= 0) 
    {
        std::cerr << "Invalid address or address not supported.\n";
        return 1;
    }

    // 3. Connect to the server
    if (connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) 
    {
        std::cerr << "Connection failed. Is the server running?\n";
        return 1;
    }

    // 4. Send data to the server
    const char* message = "Hello from the C++ client!";
    send(clientSocket, message, strlen(message), 0);
    std::cout << "Message sent to server.\n";

    // 5. Close the client socket
    close(clientSocket);
    return 0;
}
