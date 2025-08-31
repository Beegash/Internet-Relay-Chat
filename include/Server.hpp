#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <map>
#include <vector>
#include <set>
#include <sys/select.h>
#include <csignal>

static volatile sig_atomic_t g_stop = 0;


class Client;
class Channel;

class Server
{
public:
	Server(int port, const char *password);
	~Server();
	void run();

private:
	// === NETWORK OPERATIONS (ServerNetwork.cpp) ===
	void handleNewConnection(int listen_fd);
	void handleClientData(Client *client);
	void removeClient(Client *client);

	// === COMMAND PROCESSING (ServerCommands.cpp) ===
	void processCommand(Client *client, const std::string &command);
	
	// IRC Authentication Commands
	void handlePass(Client *client, const std::vector<std::string> &args);
	void handleNick(Client *client, const std::vector<std::string> &args);
	void handleUser(Client *client, const std::vector<std::string> &args);
	
	// IRC Channel Commands
	void handleJoin(Client *client, const std::vector<std::string> &args);
	void handlePart(Client *client, const std::vector<std::string> &args);
	void handlePrivmsg(Client *client, const std::vector<std::string> &args);
	void handleKick(Client *client, const std::vector<std::string> &args);
	void handleInvite(Client *client, const std::vector<std::string> &args);
	void handleTopic(Client *client, const std::vector<std::string> &args);
	void handleMode(Client *client, const std::vector<std::string> &args);
	
	// IRC Utility Commands
	void handleQuit(Client *client, const std::vector<std::string> &args);
	void handleCap(Client *client, const std::vector<std::string> &args);
	void handlePing(Client *client, const std::vector<std::string> &args);
	void handleNotice(Client *client, const std::vector<std::string> &args);
	void handleWho(Client *client, const std::vector<std::string> &args);

	// === UTILITY FUNCTIONS (ServerUtils.cpp) ===
	std::vector<std::string> splitCommand(const std::string &command);
	Client *findClientByNickname(const std::string &nickname);
	Channel *findChannel(const std::string &name);
	Channel *createChannel(const std::string &name);
	void sendWelcome(Client *client);

	// === MEMBER VARIABLES ===
	int _port;
	std::string _password;
	int _listen_fd;

	// fd_set management
	fd_set _master_set;
	fd_set _read_fds;
	int _fd_max;

	std::map<int, Client *> _clients;				  // fd -> Client*
	std::map<std::string, Client *> _clients_by_nick; // nickname -> Client*
	std::map<std::string, Channel *> _channels;		  // channel_name -> Channel*
};

#endif
 