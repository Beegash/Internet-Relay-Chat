#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

class TestClient {
private:
    int _socket;
    std::string _buffer;

public:
    TestClient() : _socket(-1) {}

    bool connect(const std::string& host, int port) {
        _socket = socket(AF_INET, SOCK_STREAM, 0);
        if (_socket < 0) {
            std::cerr << "Socket creation failed" << std::endl;
            return false;
        }

        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr);

        if (::connect(_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "Connection failed" << std::endl;
            return false;
        }

        return true;
    }

    bool send(const std::string& message) {
        std::string fullMessage = message + "\r\n";
        if (::send(_socket, fullMessage.c_str(), fullMessage.length(), 0) < 0) {
            std::cerr << "Send failed" << std::endl;
            return false;
        }
        return true;
    }

    std::string receive() {
        char buffer[512];
        int bytesRead = recv(_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) {
            return "";
        }
        buffer[bytesRead] = '\0';
        return std::string(buffer);
    }

    void close() {
        if (_socket != -1) {
            ::close(_socket);
            _socket = -1;
        }
    }

    ~TestClient() {
        close();
    }
};

int main() {
    TestClient client;
    
    // Sunucuya bağlan
    std::cout << "Connecting to server..." << std::endl;
    if (!client.connect("127.0.0.1", 6667)) {
        std::cerr << "Failed to connect to server" << std::endl;
        return 1;
    }
    std::cout << "Connected to server successfully" << std::endl;

    // Temel IRC komutlarını test et
    std::cout << "Testing PASS command..." << std::endl;
    if (!client.send("PASS default")) {
        std::cerr << "Failed to send PASS command" << std::endl;
        return 1;
    }
    std::string response = client.receive();
    std::cout << "Server response: " << response << std::endl;

    std::cout << "\nTesting NICK command..." << std::endl;
    client.send("NICK testuser");
    std::cout << "Server response: " << client.receive() << std::endl;

    std::cout << "\nTesting USER command..." << std::endl;
    client.send("USER testuser 0 * :Test User");
    std::cout << "Server response: " << client.receive() << std::endl;

    std::cout << "\nTesting JOIN command..." << std::endl;
    client.send("JOIN #test");
    std::cout << "Server response: " << client.receive() << std::endl;

    std::cout << "\nTesting PRIVMSG command..." << std::endl;
    client.send("PRIVMSG #test :Hello, world!");
    std::cout << "Server response: " << client.receive() << std::endl;

    // Test tamamlandı
    std::cout << "\nTest completed. Press Enter to quit..." << std::endl;
    std::cin.get();

    return 0;
} 
