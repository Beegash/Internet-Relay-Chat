#include "Client.hpp"
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>

Client::Client(int fd) : _fd(fd), _authenticated(false), _registered(false)
{
}

Client::~Client()
{
	if (_fd >= 0)
		close(_fd);
}

int Client::getFd() const
{
	return _fd;
}

const std::string &Client::getNickname() const
{
	return _nickname;
}

const std::string &Client::getUsername() const
{
	return _username;
}

const std::string &Client::getRealname() const
{
	return _realname;
}

bool Client::isAuthenticated() const
{
	return _authenticated;
}

bool Client::isRegistered() const
{
	return _registered;
}

void Client::setNickname(const std::string &nickname)
{
	_nickname = nickname;
}

void Client::setUsername(const std::string &username)
{
	_username = username;
}

void Client::setRealname(const std::string &realname)
{
	_realname = realname;
}

void Client::setAuthenticated(bool auth)
{
	_authenticated = auth;
}

void Client::setRegistered(bool reg)
{
	_registered = reg;
}

void Client::sendMessage(const std::string &message)
{
	if (_fd >= 0)
	{
		send(_fd, message.c_str(), message.length(), 0);
	}
}
