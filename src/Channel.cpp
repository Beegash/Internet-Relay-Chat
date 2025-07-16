#include "Channel.hpp"
#include "Client.hpp"
#include <algorithm>
#include <iostream>

Channel::Channel(const std::string &name)
	: _name(name), _inviteOnly(false), _topicRestricted(false), _userLimit(0)
{
}

Channel::~Channel()
{
    _invitedClients.clear();
    _clients.clear();
    _operators.clear();
    _banList.clear();
}

const std::string &Channel::getName() const
{
	return _name;
}

const std::string &Channel::getTopic() const
{
	return _topic;
}

void Channel::setTopic(const std::string &topic)
{
	_topic = topic;
}

void Channel::addClient(Client *client)
{
	if (client && !hasClient(client))
	{
		_clients.push_back(client);
	}
}

void Channel::removeClient(Client *client)
{
	if (client)
	{
		_clients.erase(std::remove(_clients.begin(), _clients.end(), client), _clients.end());
		removeOperator(client);
	}
}

bool Channel::hasClient(Client *client) const
{
	return std::find(_clients.begin(), _clients.end(), client) != _clients.end();
}

std::vector<Client *> Channel::getClients() const
{
	return _clients;
}

void Channel::addOperator(Client *client)
{
	if (client && !isOperator(client))
	{
		_operators.push_back(client);
	}
}

void Channel::removeOperator(Client *client)
{
	if (client)
	{
		_operators.erase(std::remove(_operators.begin(), _operators.end(), client), _operators.end());
	}
}

bool Channel::isOperator(Client *client) const
{
	return std::find(_operators.begin(), _operators.end(), client) != _operators.end();
}

std::vector<Client *> Channel::getOperators() const
{
	return _operators;
}

bool Channel::hasOperators() const
{
	return !_operators.empty();
}

void Channel::broadcast(const std::string &message, Client *sender)
{
	for (std::vector<Client *>::iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		if (*it != sender)
		{
			(*it)->sendMessage(message);
		}
	}
}

void Channel::setInviteOnly(bool inviteOnly)
{
    std::cout << "DEBUG: setInviteOnly(" << (inviteOnly ? "true" : "false") << ") for channel " << _name << std::endl;
    _inviteOnly = inviteOnly;
}

bool Channel::isInviteOnly() const
{
    std::cout << "DEBUG: isInviteOnly() for channel " << _name << " = " << (_inviteOnly ? "true" : "false") << std::endl;
    return _inviteOnly;
}

void Channel::setTopicRestricted(bool restricted)
{
	_topicRestricted = restricted;
}

bool Channel::isTopicRestricted() const
{
	return _topicRestricted;
}

void Channel::setKey(const std::string &key)
{
	_key = key;
}

const std::string &Channel::getKey() const
{
	return _key;
}

void Channel::setUserLimit(int limit)
{
	_userLimit = limit;
}

int Channel::getUserLimit() const
{
	return _userLimit;
}

void Channel::addBan(const std::string &mask)
{
    if (!isBanned(mask))
    {
        std::cout << "DEBUG: addBan " << mask << std::endl;
        _banList.push_back(mask);
    }
}

void Channel::removeBan(const std::string &mask)
{
    std::cout << "DEBUG: removeBan " << mask << std::endl;
    _banList.erase(std::remove(_banList.begin(), _banList.end(), mask), _banList.end());
}

bool Channel::isBanned(const std::string &mask) const
{
	return std::find(_banList.begin(), _banList.end(), mask) != _banList.end();
}

std::vector<std::string> Channel::getBanList() const
{
	return _banList;
}

void Channel::promoteNextOperator()
{
	if (_operators.empty() && !_clients.empty())
	{
		Client *newOp = _clients[0];
		addOperator(newOp);
		std::string nickname = newOp->getNickname();
		std::string modeMsg = ":localhost MODE " + _name + " +o " + nickname + "\r\n";
		broadcast(modeMsg);
	}
}

void Channel::addInvite(Client *client) {
    if (client && std::find(_invitedClients.begin(), _invitedClients.end(), client) == _invitedClients.end()) {
        std::cout << "DEBUG: addInvite " << client->getNickname() << std::endl;
        _invitedClients.push_back(client);
    }
}

void Channel::removeInvite(Client *client) {
    if (client) {
        std::vector<Client *>::iterator it = std::find(_invitedClients.begin(), _invitedClients.end(), client);
        if (it != _invitedClients.end()) {
            std::cout << "DEBUG: removeInvite " << client->getNickname() << std::endl;
            _invitedClients.erase(it);
        }
    }
}

bool Channel::isInvited(Client *client) const {
    return std::find(_invitedClients.begin(), _invitedClients.end(), client) != _invitedClients.end();
}
