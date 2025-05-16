#include "../include/Server.hpp"
#include "../include/Client.hpp"
#include "../include/Channel.hpp"
#include "../include/CommandHandler.hpp"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

Server& Server::getInstance() {
    static Server instance;
    return instance;
}

Server::Server() : _port(0), _password("default"), _serverSocket(-1), _serverName("ircserv") {}

Server::~Server() {
    // Tüm client'ları temizle
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        delete it->second;
        close(it->first);
    }
    _clients.clear();

    // Tüm kanalları temizle
    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
        delete it->second;
    }
    _channels.clear();

    // Server socket'i kapat
    if (_serverSocket != -1) {
        close(_serverSocket);
    }
}

void Server::init(int port, const std::string& password) {
    _port = port;
    _password = password;

    // Server socket'i oluştur
    _serverSocket = Utils::createSocket();
    if (_serverSocket < 0) {
        throw std::runtime_error("Failed to create server socket");
    }

    // Socket'i non-blocking moda al
    Utils::setNonBlocking(_serverSocket);

    // Socket'i bağla ve dinlemeye başla
    Utils::bindSocket(_serverSocket, _port);
    Utils::listenSocket(_serverSocket);

    // Server socket'i poll listesine ekle
    addToPoll(_serverSocket, POLLIN);

    Utils::log("Server initialized on port " + std::to_string(_port));
}

void Server::start() {
    Utils::log("Server started");
    
    while (true) {
        // Poll olaylarını bekle
        int pollResult = poll(&_pollfds[0], _pollfds.size(), -1);
        
        if (pollResult < 0) {
            Utils::error("Poll error");
            break;
        }

        // Poll olaylarını işle
        handlePollEvents();
    }
}

void Server::stop() {
    Utils::log("Server stopping...");
    // TODO: Graceful shutdown implementasyonu
}

void Server::addClient(int clientSocket) {
    Client* client = new Client(clientSocket);
    _clients[clientSocket] = client;
    addToPoll(clientSocket, POLLIN | POLLOUT);
    Utils::log("New client connected: " + std::to_string(clientSocket));
}

void Server::removeClient(int clientSocket) {
    // Client'ı poll listesinden çıkar
    removeFromPoll(clientSocket);
    
    // Client'ı _clients map'inden çıkar ve bellekten temizle
    std::map<int, Client*>::iterator it = _clients.find(clientSocket);
    if (it != _clients.end()) {
        delete it->second;
        _clients.erase(it);
    }
    
    close(clientSocket);
    Utils::log("Client disconnected: " + std::to_string(clientSocket));
}

void Server::addToPoll(int fd, short events) {
    pollfd pfd;
    pfd.fd = fd;
    pfd.events = events;
    pfd.revents = 0;
    _pollfds.push_back(pfd);
}

void Server::removeFromPoll(int fd) {
    for (std::vector<pollfd>::iterator it = _pollfds.begin(); it != _pollfds.end(); ++it) {
        if (it->fd == fd) {
            _pollfds.erase(it);
            break;
        }
    }
}

void Server::handlePollEvents() {
    for (size_t i = 0; i < _pollfds.size(); i++) {
        if (_pollfds[i].revents == 0) {
            continue;
        }

        if (_pollfds[i].fd == _serverSocket) {
            if (_pollfds[i].revents & POLLIN) {
                struct sockaddr_in clientAddr;
                socklen_t clientLen = sizeof(clientAddr);
                
                int clientSocket = accept(_serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
                if (clientSocket < 0) {
                    Utils::error("Accept failed");
                    continue;
                }

                Utils::setNonBlocking(clientSocket);
                addClient(clientSocket);
                Utils::log("New client accepted: " + std::to_string(clientSocket));
            }
        } else {
            Client* client = getClient(_pollfds[i].fd);
            if (!client) continue;

            if (_pollfds[i].revents & POLLIN) {
                char buffer[512];
                int bytesRead = recv(_pollfds[i].fd, buffer, sizeof(buffer) - 1, 0);
                
                if (bytesRead <= 0) {
                    Utils::log("Client disconnected: " + std::to_string(_pollfds[i].fd));
                    removeClient(_pollfds[i].fd);
                    continue;
                }

                buffer[bytesRead] = '\0';
                Utils::log("Received from client " + std::to_string(_pollfds[i].fd) + ": " + buffer);
                client->appendToBuffer(buffer);
                
                std::vector<std::string> messages = client->getCompleteMessages();
                for (std::vector<std::string>::iterator it = messages.begin(); it != messages.end(); ++it) {
                    Utils::log("Processing command: " + *it);
                    CommandHandler::getInstance().parseAndHandleCommand(client, *it);
                }
            }
            if (_pollfds[i].revents & (POLLHUP | POLLERR)) {
                Utils::log("Client connection error: " + std::to_string(_pollfds[i].fd));
                removeClient(_pollfds[i].fd);
            }
        }
    }
}

void Server::broadcast(const std::string& message, int excludeFd) {
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if (it->first != excludeFd) {
            sendToClient(it->first, message);
        }
    }
}

void Server::sendToClient(int clientFd, const std::string& message) {
    std::string fullMessage = message + "\r\n";
    if (send(clientFd, fullMessage.c_str(), fullMessage.length(), 0) < 0) {
        Utils::error("Failed to send message to client " + std::to_string(clientFd));
    }
}

Client* Server::getClient(int clientSocket) {
    std::map<int, Client*>::iterator it = _clients.find(clientSocket);
    return (it != _clients.end()) ? it->second : NULL;
}

Client* Server::getClientByNickname(const std::string& nickname) {
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if (it->second->getNickname() == nickname) {
            return it->second;
        }
    }
    return NULL;
}

Channel* Server::getChannel(const std::string& name) {
    std::map<std::string, Channel*>::iterator it = _channels.find(name);
    return (it != _channels.end()) ? it->second : NULL;
}

void Server::createChannel(const std::string& name, Client* creator) {
    if (_channels.find(name) == _channels.end()) {
        Channel* channel = new Channel(name);
        _channels[name] = channel;
        channel->addClient(creator);
        channel->addOperator(creator);
    }
}

void Server::removeChannel(const std::string& name) {
    std::map<std::string, Channel*>::iterator it = _channels.find(name);
    if (it != _channels.end()) {
        delete it->second;
        _channels.erase(it);
    }
} 
