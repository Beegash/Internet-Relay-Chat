#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <map>
#include <vector>
#include <poll.h>
#include "Utils.hpp"

class Client;
class Channel;

class Server {
private:
    int _port;
    std::string _password;
    int _serverSocket;
    std::vector<pollfd> _pollfds;
    std::map<int, Client*> _clients;
    std::map<std::string, Channel*> _channels;
    std::string _serverName;

    // Private constructor (Singleton pattern)
    Server();
    Server(const Server& other);
    Server& operator=(const Server& other);

public:
    // Singleton instance
    static Server& getInstance();
    ~Server();

    // Server başlatma ve çalıştırma
    void init(int port, const std::string& password);
    void start();
    void stop();

    // Client yönetimi
    void addClient(int clientSocket);
    void removeClient(int clientSocket);
    Client* getClient(int clientSocket);
    Client* getClientByNickname(const std::string& nickname);

    // Kanal yönetimi
    Channel* getChannel(const std::string& name);
    void createChannel(const std::string& name, Client* creator);
    void removeChannel(const std::string& name);

    // Getter'lar
    int getPort() const { return _port; }
    std::string getPassword() const { return _password; }
    int getServerSocket() const { return _serverSocket; }
    const std::string& getServerName() const { return _serverName; }

    // Poll yönetimi
    void addToPoll(int fd, short events);
    void removeFromPoll(int fd);
    void handlePollEvents();

    // Mesaj işleme
    void broadcast(const std::string& message, int excludeFd = -1);
    void sendToClient(int clientFd, const std::string& message);
};

#endif 
