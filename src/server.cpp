#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <string>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

#define PORT 27015
#define BUFLEN 8192

std::vector<SOCKET> clientSockets; // List of connected clients
std::mutex clientMutex;            // Protect access to clientSockets
std::atomic<bool> running(true);  // Shared flag to stop threads gracefully
const std::string name = "Server";

// Function to broadcast messages to all clients except the sender
void broadcastMessage(const std::string& message, SOCKET sender) {
    std::lock_guard<std::mutex> lock(clientMutex);
    for (SOCKET client : clientSockets) {
        if (client != sender) {
            if (send(client, message.c_str(), (int)message.size(), 0) == SOCKET_ERROR) {
                std::cerr << "Send failed: " << WSAGetLastError() << std::endl;
            }
        }
    }
}

// Function to handle communication with a single client
void handleClient(SOCKET clientSocket) {
    char recvbuf[BUFLEN];
    int bytesReceived;

    while (running) {
        bytesReceived = recv(clientSocket, recvbuf, BUFLEN, 0);
        if (bytesReceived > 0) {
            std::string message(recvbuf, bytesReceived);
            std::cout << message << std::endl;
            broadcastMessage(message, clientSocket);
        } else if (bytesReceived == 0 || WSAGetLastError() == WSAECONNRESET || WSAGetLastError() == WSAEINTR || WSAGetLastError() == 0) {
            std::cerr << "Client disconnected.\n";
            break;
        } else {
            std::cerr << "Recv failed with error: " << WSAGetLastError() << std::endl;
            break;
        }
    }

    // Remove client from the list and close the socket
    std::lock_guard<std::mutex> lock(clientMutex);
    clientSockets.erase(std::remove(clientSockets.begin(), clientSockets.end(), clientSocket), clientSockets.end());
    closesocket(clientSocket);
}

// Main function to accept and manage client connections
void acceptClients(SOCKET serverSocket) {
    while (running) {
        SOCKET clientSocket = accept(serverSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            if (!running) break; // Graceful shutdown
            std::cerr << "Accept failed: " << WSAGetLastError() << std::endl;
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(clientMutex);
            clientSockets.push_back(clientSocket);
        }

        std::cout << "New client connected!" << std::endl;
        std::thread clientThread(handleClient, clientSocket);
        clientThread.detach();
    }

    // Shutdown all client sockets
    std::lock_guard<std::mutex> lock(clientMutex);
    for (SOCKET client : clientSockets) {
        shutdown(client, SD_BOTH);
        closesocket(client);
    }
    clientSockets.clear();
}

int main() {
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return 1;
    }

    // Create server socket
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // Bind server socket
    sockaddr_in service;
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = INADDR_ANY;
    service.sin_port = htons(PORT);

    if (bind(serverSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
        std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    // Listen for incoming connections
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server listening on port " << PORT << std::endl;

    // Start accepting clients in a separate thread
    std::thread acceptThread(acceptClients, serverSocket);

    // Main thread handles server shutdown
    std::string command;
    std::string commandtmp;
    while (true) {
        std::getline(std::cin, command);
        if (command == "/exit") {
            running = false;
            break;
        }
        else if (command == "/clients") {
            std::lock_guard<std::mutex> lock(clientMutex);
            std::cout << "Number of connected clients: " << clientSockets.size() << std::endl;
        }
        else if (command.substr(0, 10) == "/broadcast") {
            std::string message = name + ": " + command.substr(11);
            broadcastMessage(message, INVALID_SOCKET);
        }
        else {
            std::cout << "Unknown command: " << command << std::endl;
        }
    }

    // Shutdown
    std::cout << "Shutting down server..." << std::endl;
    running = false;

    // Close the server socket
    shutdown(serverSocket, SD_SEND);
    closesocket(serverSocket);

    // Wait for accept thread to exit
    acceptThread.join();

    WSACleanup();
    std::cout << "Server stopped." << std::endl;
    return 0;
}
