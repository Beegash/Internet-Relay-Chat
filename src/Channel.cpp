#include "../include/Channel.hpp"
#include "../include/Client.hpp"
#include "../include/Server.hpp"
#include <sstream>

Channel::Channel(const std::string& name) : 
    _name(name),
    _inviteOnly(false),
    _topicProtected(false),
    _userLimit(0) {
}

Channel::~Channel() {
    // Tüm client'ları kanaldan çıkar
    for (std::set<Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        (*it)->leaveChannel(this);
    }
    _clients.clear();
    _operators.clear();
}

void Channel::setTopic(const std::string& topic) {
    _topic = topic;
}

void Channel::setKey(const std::string& key) {
    _key = key;
}

void Channel::addClient(Client* client) {
    if (client) {
        _clients.insert(client);
        client->joinChannel(this);
    }
}

void Channel::removeClient(Client* client) {
    if (client) {
        _clients.erase(client);
        _operators.erase(client);
        client->leaveChannel(this);
    }
}

bool Channel::hasClient(Client* client) const {
    return client && _clients.find(client) != _clients.end();
}

bool Channel::isOperator(Client* client) const {
    return client && _operators.find(client) != _operators.end();
}

void Channel::addOperator(Client* client) {
    if (client && hasClient(client)) {
        _operators.insert(client);
    }
}

void Channel::removeOperator(Client* client) {
    if (client) {
        _operators.erase(client);
    }
}

void Channel::setMode(char mode, bool value, const std::string& param) {
    switch (mode) {
        case 'i': // Invite-only
            _inviteOnly = value;
            break;
        case 't': // Topic protection
            _topicProtected = value;
            break;
        case 'k': // Channel key
            if (value) {
                _key = param;
            } else {
                _key.clear();
            }
            break;
        case 'l': // User limit
            if (value && !param.empty()) {
                _userLimit = std::atoi(param.c_str());
            } else {
                _userLimit = 0;
            }
            break;
    }
}

std::string Channel::getModes() const {
    std::stringstream ss;
    ss << "+";
    
    if (_inviteOnly) ss << "i";
    if (_topicProtected) ss << "t";
    if (!_key.empty()) ss << "k";
    if (_userLimit > 0) ss << "l";
    
    return ss.str();
}

void Channel::broadcast(const std::string& message, Client* exclude) {
    for (std::set<Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if (*it != exclude) {
            sendToClient(*it, message);
        }
    }
}

void Channel::sendToClient(Client* client, const std::string& message) {
    if (client) {
        Server::getInstance().sendToClient(client->getSocket(), message);
    }
} 
