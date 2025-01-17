#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <atomic>

#pragma comment(lib, "Ws2_32.lib")

#define BUFLEN 8192
#define PORT 27015

std::atomic<bool> running(true); // Shared flag to stop threads gracefully
std::string name;
// Thread function to receive messages from the server
void messageReceiver(SOCKET clientSocket) {
    char recvbuf[BUFLEN];
    int bytesReceived;

    while (running) {
        bytesReceived = recv(clientSocket, recvbuf, BUFLEN, 0);

        if (bytesReceived > 0) {
            std::cout.write(recvbuf, bytesReceived);
            std::cout << std::endl;
        } 
        else if (bytesReceived == 0) {
            std::cerr << "\nConnection closed" << std::endl;
            running = false;
            break;
        } 
        else if (bytesReceived == SOCKET_ERROR) {
            std::cerr << "\nRecv failed with error: " << WSAGetLastError() << std::endl;
            running = false;
            break;
        }
    }
}

int main(int argc, char** argv) {
    // INIT
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return 1;
    }
    name = argv[2];
    name += ": ";

    // SOCKET
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // SETUP SERVER CONNECTION
    sockaddr_in service;
    service.sin_family = AF_INET;
    InetPton(AF_INET, argv[1], &service.sin_addr.s_addr);
    service.sin_port = htons(PORT);

    if (connect(clientSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
        std::cerr << "Connection failed: " << WSAGetLastError() << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to server.\n";

    // START RECEIVER THREAD
    std::thread receiver(messageReceiver, clientSocket);

    // MAIN THREAD: Handle user input and send messages
    while (running) {
        char sendbuf[BUFLEN];
        std::string msg;
        std::getline(std::cin, msg);

        if (msg.empty()) continue;
        if (msg == "/exit") {
            running = false;
            break;
        }

        for (int i = 0; i < name.size(); i++) sendbuf[i] = name[i];
        for (int i = 0; i < msg.size(); i++) sendbuf[i + name.size()] = msg[i];

        if (send(clientSocket, sendbuf, (int)strlen(sendbuf), 0) == SOCKET_ERROR) {
            std::cerr << "Send failed: " << WSAGetLastError() << std::endl;
            running = false;
            break;
        }
        memset(sendbuf, 0, sizeof sendbuf);
    }

    // CLEANUP
    shutdown(clientSocket, SD_BOTH);
    receiver.join();
    closesocket(clientSocket);
    WSACleanup();

    std::cout << "Client disconnected.\n";
    return 0;
}