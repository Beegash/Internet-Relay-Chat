#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>

class Server;

class Client
{
public:
	Client(int fd);
	~Client();

	int getFd() const;
	const std::string &getNickname() const;
	const std::string &getUsername() const;
	const std::string &getRealname() const;
	bool isAuthenticated() const;
	bool isRegistered() const;

	void setNickname(const std::string &nickname);
	void setUsername(const std::string &username);
	void setRealname(const std::string &realname);
	void setAuthenticated(bool auth);
	void setRegistered(bool reg);

	void sendMessage(const std::string &message);

private:
	int _fd;
	std::string _nickname;
	std::string _username;
	std::string _realname;
	bool _authenticated;
	bool _registered;
	std::string _buffer;

	friend class Server;
};

#endif
