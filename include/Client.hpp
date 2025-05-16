#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <vector>
#include <set>
#include "Utils.hpp"

class Channel;

class Client {
private:
    int _socket;
    std::string _nickname;
    std::string _username;
    std::string _realname;
    std::string _hostname;
    std::string _buffer;
    bool _isAuthenticated;
    bool _isRegistered;
    std::set<Channel*> _channels;

public:
    Client(int socket);
    ~Client();

    // Getter'lar
    int getSocket() const { return _socket; }
    std::string getNickname() const { return _nickname; }
    std::string getUsername() const { return _username; }
    std::string getRealname() const { return _realname; }
    std::string getHostname() const { return _hostname; }
    bool isAuthenticated() const { return _isAuthenticated; }
    bool isRegistered() const { return _isRegistered; }
    const std::set<Channel*>& getChannels() const { return _channels; }

    // Setter'lar
    void setNickname(const std::string& nickname);
    void setUsername(const std::string& username);
    void setRealname(const std::string& realname);
    void setHostname(const std::string& hostname);
    void setAuthenticated(bool value) { _isAuthenticated = value; }
    void setRegistered(bool value) { _isRegistered = value; }

    // Kanal işlemleri
    void joinChannel(Channel* channel);
    void leaveChannel(Channel* channel);
    bool isInChannel(const std::string& channelName) const;

    // Mesaj işleme
    void appendToBuffer(const std::string& data);
    std::vector<std::string> getCompleteMessages();
    void clearBuffer() { _buffer.clear(); }

    // Kimlik doğrulama
    bool authenticate(const std::string& password);
    bool registerUser(const std::string& nickname, const std::string& username, const std::string& realname);

    // Mesaj formatı
    std::string getPrefix() const;
};

#endif 
