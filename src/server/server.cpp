#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <vector>
#include <algorithm>
#include <csignal>
#include <cerrno>
#include <stdexcept>
#include <string>
#include "internal/logger.h"
#include "spdlog/spdlog.h"

volatile std::sig_atomic_t shuttingDown = 0;

void signalHandler(int sig)
{
    shuttingDown = 1;
}

int startServer(int port)
{
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (serverSocket < 0)
    {
        throw std::runtime_error(std::string("could not create socket: ") + std::strerror(errno));
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);

    serverAddress.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
        throw std::runtime_error("port already in use");
    }

    if (listen(serverSocket,  SOMAXCONN) < 0)
    {
        const std::string error = std::strerror(errno);
        close(serverSocket);
        throw std::runtime_error("could not listen: " + error);
    }

    std::vector<int> clients;
    signal(SIGINT, signalHandler);

    while (true)
    {
        fd_set readSet;
        FD_ZERO(&readSet);

        FD_SET(serverSocket, &readSet);
        int maxFd = serverSocket;

        for (int client : clients)
        {
            FD_SET(client, &readSet);
            maxFd = std::max(maxFd, client);
        }

        timeval timeout{};
        timeout.tv_sec = 1;
        int result = select(maxFd + 1, &readSet, nullptr, nullptr, &timeout);

        if (shuttingDown == 1)
        {
            break;
        }

        if (result < 0)
        {
            SPDLOG_ERROR("select failed: {}", std::strerror(errno));
            break;
        }

        if (FD_ISSET(serverSocket, &readSet))
        {
            sockaddr_in clientAddress{};
            socklen_t clientAddressLength = sizeof(clientAddress);
            int clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddress, &clientAddressLength);

            if (clientSocket != -1)
            {
                clients.push_back(clientSocket);
                char clientIP[INET_ADDRSTRLEN];
                inet_ntop(
                    AF_INET,
                    &clientAddress.sin_addr,
                    clientIP,
                    sizeof(clientIP));

                int clientPort = ntohs(clientAddress.sin_port);
                // replace with write to client
                SPDLOG_INFO("Client connected from {}:{} (socket {})", clientIP, clientPort, clientSocket);
            }
            else
            {
                SPDLOG_ERROR("accept failed: {}", std::strerror(errno));
            }
        }

        for (auto it = clients.begin(); it != clients.end();)
        {
            int clientSoc = *it;

            if (FD_ISSET(clientSoc, &readSet))
            {
                char buffer[1024];
                ssize_t bytes = recv(clientSoc, buffer, sizeof(buffer), 0);

                if (bytes <= 0)
                {
                    close(clientSoc);
                    it = clients.erase(it);
                    SPDLOG_INFO("Client disconnected (socket {})", clientSoc);
                    continue;
                }
                SPDLOG_INFO("Message from client on socket {}: {}", clientSoc,
                            std::string(buffer, static_cast<std::size_t>(bytes)));
            }
            ++it;
        }
    }
    for (int client : clients)
    {
        const char message[] = "Server is shutting down\n";
        send(client, message, sizeof(message) - 1, 0);

        shutdown(client, SHUT_RDWR);
        close(client);
    }
    close(serverSocket);
    SPDLOG_INFO("Ok Bie");

    return 0;
}