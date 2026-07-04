#include <iostream>
#include <cstring>
#include <string>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "niniBUS.h"

int niniBUS::init_niniServer()
{
    // 1. Create a socket (IPv4, TCP)
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) 
    {
        std::cerr << "Failed to create socket.\n";
        return 1;
    }

    // 2. Define the server address structure
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);      // Port 8080
    serverAddress.sin_addr.s_addr = INADDR_ANY; // Accept connections on any IP

    // 3. Bind the socket to the port and IP
        if (::bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0)
    {
        std::cerr << "Bind failed.\n";
        return 1;
    }

    // 4. Listen for incoming client connections (queue up to 5)
    if (listen(serverSocket, 5) < 0) 
    {
        std::cerr << "Listen failed.\n";
        return 1;
    }

    std::cout << "Server is listening on port 8080...\n";

    bool keepRunning = true;

    // Accept loop: allow multiple sequential clients to connect
    while (keepRunning)
    {
        // 5. Accept an incoming client connection (blocks until client connects)
        int clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket < 0)
        {
            std::cerr << "Accept failed. Continuing to listen...\n";
            continue; // try accepting again
        }

        std::cout << "Client connected successfully!\n";

        // 6. Receive data from the client in a loop until the client disconnects
        char buffer[1024];
        while (true)
        {
            ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
            if (bytesRead > 0)
            {
                buffer[bytesRead] = '\0';
                std::cout << "Message from client: " << buffer << std::endl;

                // If client sends "exit" close the connection gracefully (client disconnect)
                if (std::string(buffer) == "exit")
                {
                    std::cout << "Client requested to close the connection." << std::endl;
                    break; // break client loop, keep server running
                }

                // If client sends "shutdown" then shut down the server entirely
                if (std::string(buffer) == "shutdown")
                {
                    std::cout << "Shutdown command received. Stopping server." << std::endl;
                    keepRunning = false;
                    break; // break client loop
                }
            }
            else if (bytesRead == 0)
            {
                // peer closed connection
                std::cout << "Client disconnected." << std::endl;
                break;
            }
            else
            {
                std::cerr << "Recv failed." << std::endl;
                break;
            }
        }

        // close client socket and, if requested, exit accept loop
        close(clientSocket);
    }

    // 7. Clean up and close server socket
    close(serverSocket);
    return 0;
}
int niniBUS::init_niniServerIPC()
{
    // Implementation for IPC initialization
    cout << "Initializing IPC..." << endl;
    return 0;
}

int niniBUS::init_niniServerThread()
{
    // Implementation for Thread initialization
    cout << "Initializing Thread..." << endl;
    return 0;
}
