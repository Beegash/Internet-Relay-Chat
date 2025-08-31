#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <vector>
#include <map>
#include <set>

class Client;

class Channel
{
public:
	Channel(const std::string &name);
	~Channel();

	const std::string &getName() const;
	const std::string &getTopic() const;
	void setTopic(const std::string &topic);

	void addClient(Client *client);
	void removeClient(Client *client);
	bool hasClient(Client *client) const;
	std::vector<Client *> getClients() const;

	void addOperator(Client *client);
	void removeOperator(Client *client);
	bool isOperator(Client *client) const;
	std::vector<Client *> getOperators() const;
	bool hasOperators() const;
	void promoteNextOperator();



	void broadcast(const std::string &message, Client *sender = NULL);

	// Mode işlemleri
	void setInviteOnly(bool inviteOnly);
	bool isInviteOnly() const;
	void setTopicRestricted(bool restricted);
	bool isTopicRestricted() const;
	void setKey(const std::string &key);
	const std::string &getKey() const;
	void setUserLimit(int limit);
	int getUserLimit() const;

	// Davet yönetimi (+i)
	void addInvitation(const std::string &nickname);
	void removeInvitation(const std::string &nickname);
	bool isInvited(const std::string &nickname) const;

	// Ban listesi yönetimi
	void addBan(const std::string &mask);
	void removeBan(const std::string &mask);
	bool isBanned(const std::string &mask) const;
	std::vector<std::string> getBanList() const;

private:
	std::string _name;
	std::string _topic;
	std::vector<Client *> _clients;
	std::vector<Client *> _operators;

	// Channel modes
	bool _inviteOnly;
	bool _topicRestricted;
	std::string _key;
	int _userLimit;
	std::vector<std::string> _banList;

	// Invite-only için davet listesi
	std::set<std::string> _invitedNicks;
};

#endif
