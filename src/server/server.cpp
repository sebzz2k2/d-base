#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "internal/logger.h"
#include "spdlog/spdlog.h"

constexpr int MAX_EVENTS = 64;
volatile std::sig_atomic_t shuttingDown = 0;

void signalHandler(int sig)
{
    shuttingDown = 1;
}

int startServer(int port)
{
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0)
        throw std::runtime_error(std::string("could not create socket: ") + std::strerror(errno));

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

    signal(SIGINT, signalHandler);
    const int epollFd = epoll_create1(0);
    if (epollFd == -1)
    {
        const std::string error = std::strerror(errno);
        close(serverSocket);
        throw std::runtime_error("could not create epoll: " + error);
    }

    epoll_event event{};
    // TODO: explore EPOLLET later
    event.events = EPOLLIN;
    event.data.fd = serverSocket;
    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, serverSocket, &event) == -1)
    {
        const std::string error = std::strerror(errno);
        close(epollFd);
        close(serverSocket);
        throw std::runtime_error("could not register listening socket: " + error);
    }

    epoll_event events[MAX_EVENTS];
    while (!shuttingDown)
    {
        const int eventCount = epoll_wait(epollFd, events, MAX_EVENTS, -1);
        if (eventCount == -1)
        {
            if (errno == EINTR && shuttingDown) break;
            SPDLOG_ERROR("epoll_wait failed: {}", std::strerror(errno));
            break;
        }

        for (int i = 0; i < eventCount; ++i)
        {
            if (events[i].data.fd == serverSocket)
            {
                sockaddr_in clientAddress{};
                socklen_t addressLength = sizeof(clientAddress);
                const int clientSock = accept(
                    serverSocket, (struct sockaddr *)&clientAddress, &addressLength);
                if (clientSock == -1)
                {
                    SPDLOG_ERROR("accept failed: {}", std::strerror(errno));
                    continue;
                }

                event = {};
                
                // TODO: explore EPOLLET later
                event.events = EPOLLIN;
                event.data.fd = clientSock;
                if (epoll_ctl(epollFd, EPOLL_CTL_ADD, clientSock, &event) == -1)
                {
                    SPDLOG_ERROR("could not register client socket {}: {}",
                                 clientSock, std::strerror(errno));
                    close(clientSock);
                    continue;
                }

                char clientIp[INET_ADDRSTRLEN]{};
                inet_ntop(AF_INET, &clientAddress.sin_addr, clientIp, sizeof(clientIp));
                SPDLOG_INFO("Client connected from {}:{} (socket {})",
                            clientIp, ntohs(clientAddress.sin_port), clientSock);
            }
            else
            {
                const int clientSock = events[i].data.fd;
                char buffer[1024];
                const ssize_t bytes = recv(clientSock, buffer, sizeof(buffer), 0);

                if (bytes > 0)
                {
                    SPDLOG_INFO("Message from client on socket {}: {}", clientSock,
                                std::string(buffer, static_cast<std::size_t>(bytes)));
                }
                else if (bytes == 0)
                {
                    epoll_ctl(epollFd, EPOLL_CTL_DEL, clientSock, nullptr);
                    close(clientSock);
                    SPDLOG_INFO("Client disconnected (socket {})", clientSock);
                }
                else if (errno != EAGAIN && errno != EWOULDBLOCK)
                {
                    SPDLOG_ERROR("recv failed on socket {}: {}",
                                 clientSock, std::strerror(errno));
                    epoll_ctl(epollFd, EPOLL_CTL_DEL, clientSock, nullptr);
                    close(clientSock);
                }
            }
        }
    }

    close(epollFd);
    close(serverSocket);
    SPDLOG_INFO("Ok Bie");
    return 0;
}
