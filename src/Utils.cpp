#include "../include/Utils.hpp"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctime>

std::vector<std::string> Utils::split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    
    while (std::getline(tokenStream, token, delimiter)) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

std::string Utils::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

bool Utils::isNumeric(const std::string& str) {
    if (str.empty()) return false;
    for (size_t i = 0; i < str.length(); i++) {
        if (!isdigit(str[i])) return false;
    }
    return true;
}

void Utils::error(const std::string& message) {
    std::cerr << "Error: " << message << std::endl;
}

void Utils::log(const std::string& message) {
    std::cout << "Log: " << message << std::endl;
}

int Utils::createSocket() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        error("Socket creation failed");
        return -1;
    }
    return sockfd;
}

void Utils::setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        error("fcntl F_GETFL failed");
        return;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        error("fcntl F_SETFL failed");
    }
}

void Utils::bindSocket(int sockfd, int port) {
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        error("Bind failed");
        close(sockfd);
        exit(1);
    }
}

void Utils::listenSocket(int sockfd) {
    if (listen(sockfd, SOMAXCONN) < 0) {
        error("Listen failed");
        close(sockfd);
        exit(1);
    }
}

std::string Utils::formatMessage(const std::string& prefix, const std::string& command, const std::string& params) {
    std::string message;
    if (!prefix.empty()) {
        message += ":" + prefix + " ";
    }
    message += command;
    if (!params.empty()) {
        message += " " + params;
    }
    message += "\r\n";
    return message;
}

bool Utils::isValidNickname(const std::string& nickname) {
    if (nickname.empty() || nickname.length() > 9) return false;
    
    // Nickname sadece harf, rakam ve özel karakterler içerebilir
    for (size_t i = 0; i < nickname.length(); i++) {
        char c = nickname[i];
        if (!isalnum(c) && c != '-' && c != '_' && c != '[' && c != ']' && c != '\\' && c != '`' && c != '^' && c != '{' && c != '}') {
            return false;
        }
    }
    return true;
}

bool Utils::isValidChannelName(const std::string& channel) {
    if (channel.empty() || channel.length() > 50) return false;
    
    // Kanal ismi # veya & ile başlamalı
    if (channel[0] != '#' && channel[0] != '&') return false;
    
    // Kanal ismi boşluk, virgül, CTRL G (7) veya null karakter içermemeli
    for (size_t i = 1; i < channel.length(); i++) {
        char c = channel[i];
        if (c == ' ' || c == ',' || c == 7 || c == 0) {
            return false;
        }
    }
    return true;
}

std::string Utils::getServerCreationTime() {
    time_t now = time(0);
    struct tm tstruct;
    char buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tstruct);
    return std::string(buf);
} 
