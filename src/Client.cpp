#include "../include/Client.hpp"
#include "../include/Channel.hpp"
#include <sstream>
#include <algorithm>

Client::Client(int socket) : 
    _socket(socket),
    _isAuthenticated(false),
    _isRegistered(false) {
}

Client::~Client() {
    // Tüm kanallardan çık
    for (std::set<Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
        (*it)->removeClient(this);
    }
    _channels.clear();
}

void Client::setNickname(const std::string& nickname) {
    if (Utils::isValidNickname(nickname)) {
        _nickname = nickname;
    }
}

void Client::setUsername(const std::string& username) {
    _username = username;
}

void Client::setRealname(const std::string& realname) {
    _realname = realname;
}

void Client::setHostname(const std::string& hostname) {
    _hostname = hostname;
}

void Client::joinChannel(Channel* channel) {
    if (channel) {
        _channels.insert(channel);
    }
}

void Client::leaveChannel(Channel* channel) {
    if (channel) {
        _channels.erase(channel);
    }
}

bool Client::isInChannel(const std::string& channelName) const {
    for (std::set<Channel*>::const_iterator it = _channels.begin(); it != _channels.end(); ++it) {
        if ((*it)->getName() == channelName) {
            return true;
        }
    }
    return false;
}

void Client::appendToBuffer(const std::string& data) {
    _buffer += data;
}

std::vector<std::string> Client::getCompleteMessages() {
    std::vector<std::string> messages;
    size_t pos = 0;
    size_t end;

    while ((end = _buffer.find("\r\n", pos)) != std::string::npos) {
        messages.push_back(_buffer.substr(pos, end - pos));
        pos = end + 2;
    }

    if (pos < _buffer.length()) {
        _buffer = _buffer.substr(pos);
    } else {
        _buffer.clear();
    }

    return messages;
}

bool Client::authenticate(const std::string& password) {
    (void)password;
    // TODO: Gerçek kimlik doğrulama mantığı burada olacak
    _isAuthenticated = true;
    return true;
}

bool Client::registerUser(const std::string& nickname, const std::string& username, const std::string& realname) {
    if (!Utils::isValidNickname(nickname)) {
        return false;
    }

    setNickname(nickname);
    setUsername(username);
    setRealname(realname);
    _isRegistered = true;
    return true;
}

std::string Client::getPrefix() const {
    std::stringstream ss;
    ss << _nickname << "!" << _username << "@" << _hostname;
    return ss.str();
} 
