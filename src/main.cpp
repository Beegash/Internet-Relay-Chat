#include "../include/Utils.hpp"
#include "../include/Server.hpp"
#include <iostream>
#include <cstdlib>

void printUsage() {
    std::cout << "Usage: ./ircserv <port> <password>" << std::endl;
    exit(1);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        printUsage();
    }

    // Port numarasını kontrol et
    if (!Utils::isNumeric(argv[1])) {
        Utils::error("Invalid port number");
        printUsage();
    }

    int port = std::atoi(argv[1]);
    std::string password = argv[2];

    try {
        // Server'ı başlat
        Server& server = Server::getInstance();
        server.init(port, password);
        server.start();
    } catch (const std::exception& e) {
        Utils::error(e.what());
        return 1;
    }

    return 0;
} 
