#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <map>
#include <set>
#include "Utils.hpp"

class Client;

class Channel {
private:
    std::string _name;
    std::string _topic;
    std::string _key;
    std::set<Client*> _clients;
    std::set<Client*> _operators;
    bool _inviteOnly;
    bool _topicProtected;
    int _userLimit;

public:
    Channel(const std::string& name);
    ~Channel();

    // Getter'lar
    std::string getName() const { return _name; }
    std::string getTopic() const { return _topic; }
    std::string getKey() const { return _key; }
    bool isInviteOnly() const { return _inviteOnly; }
    bool isTopicProtected() const { return _topicProtected; }
    int getUserLimit() const { return _userLimit; }
    size_t getClientCount() const { return _clients.size(); }
    const std::set<Client*>& getClients() const { return _clients; }
    const std::set<Client*>& getOperators() const { return _operators; }

    // Setter'lar
    void setTopic(const std::string& topic);
    void setKey(const std::string& key);
    void setInviteOnly(bool value) { _inviteOnly = value; }
    void setTopicProtected(bool value) { _topicProtected = value; }
    void setUserLimit(int limit) { _userLimit = limit; }

    // Client yönetimi
    void addClient(Client* client);
    void removeClient(Client* client);
    bool hasClient(Client* client) const;
    bool isOperator(Client* client) const;

    // Operatör yönetimi
    void addOperator(Client* client);
    void removeOperator(Client* client);

    // Kanal modları
    void setMode(char mode, bool value, const std::string& param = "");
    std::string getModes() const;

    // Mesaj işleme
    void broadcast(const std::string& message, Client* exclude = NULL);
    void sendToClient(Client* client, const std::string& message);
};

#endif 
