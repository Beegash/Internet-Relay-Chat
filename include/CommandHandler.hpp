#ifndef COMMANDHANDLER_HPP
#define COMMANDHANDLER_HPP

#include <string>
#include <map>
#include <vector>
#include "Client.hpp"
#include "Channel.hpp"

class CommandHandler {
private:
    typedef void (CommandHandler::*CommandFunction)(Client* client, const std::vector<std::string>& params);
    std::map<std::string, CommandFunction> _commands;

    // Komut işleme fonksiyonları
    void handlePass(Client* client, const std::vector<std::string>& params);
    void handleNick(Client* client, const std::vector<std::string>& params);
    void handleUser(Client* client, const std::vector<std::string>& params);
    void handleJoin(Client* client, const std::vector<std::string>& params);
    void handlePart(Client* client, const std::vector<std::string>& params);
    void handlePrivmsg(Client* client, const std::vector<std::string>& params);
    void handleNotice(Client* client, const std::vector<std::string>& params);
    void handleKick(Client* client, const std::vector<std::string>& params);
    void handleInvite(Client* client, const std::vector<std::string>& params);
    void handleTopic(Client* client, const std::vector<std::string>& params);
    void handleMode(Client* client, const std::vector<std::string>& params);
    void handleQuit(Client* client, const std::vector<std::string>& params);
    void handlePing(Client* client, const std::vector<std::string>& params);
    void handlePong(Client* client, const std::vector<std::string>& params);
    void handleCap(Client* client, const std::vector<std::string>& params);

    // Yardımcı fonksiyonlar
    void sendError(Client* client, const std::string& code, const std::string& message);
    void sendReply(Client* client, const std::string& code, const std::string& message);
    bool validateParams(Client* client, const std::vector<std::string>& params, size_t required);

    CommandHandler();
    CommandHandler(const CommandHandler& other);
    CommandHandler& operator=(const CommandHandler& other);

public:
    static CommandHandler& getInstance();

    // Komut işleme
    void handleCommand(Client* client, const std::string& command, const std::vector<std::string>& params);
    void parseAndHandleCommand(Client* client, const std::string& message);

    virtual ~CommandHandler();
};

#endif
