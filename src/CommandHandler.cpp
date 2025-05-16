#include "../include/CommandHandler.hpp"
#include "../include/Server.hpp"
#include "../include/Utils.hpp"
#include <sstream>

CommandHandler& CommandHandler::getInstance() {
    static CommandHandler instance;
    return instance;
}

CommandHandler::CommandHandler() {
    // Komut fonksiyonlarını map'e ekle
    _commands["CAP"] = &CommandHandler::handleCap;
    _commands["PING"] = &CommandHandler::handlePing;
    _commands["PASS"] = &CommandHandler::handlePass;
    _commands["NICK"] = &CommandHandler::handleNick;
    _commands["USER"] = &CommandHandler::handleUser;
    _commands["JOIN"] = &CommandHandler::handleJoin;
    _commands["PART"] = &CommandHandler::handlePart;
    _commands["PRIVMSG"] = &CommandHandler::handlePrivmsg;
    _commands["NOTICE"] = &CommandHandler::handleNotice;
    _commands["KICK"] = &CommandHandler::handleKick;
    _commands["INVITE"] = &CommandHandler::handleInvite;
    _commands["TOPIC"] = &CommandHandler::handleTopic;
    _commands["MODE"] = &CommandHandler::handleMode;
    _commands["QUIT"] = &CommandHandler::handleQuit;
    _commands["PONG"] = &CommandHandler::handlePong;
}

CommandHandler::~CommandHandler() {}

void CommandHandler::handleCommand(Client* client, const std::string& command, const std::vector<std::string>& params) {
    std::string cmd = Utils::trim(command);
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

    // Komut sırasını kontrol et
    if (cmd == "NICK" || cmd == "USER") {
        if (!client->isAuthenticated()) {
            sendError(client, "451", ":You must authenticate first");
            return;
        }
    }

    std::map<std::string, CommandFunction>::iterator it = _commands.find(cmd);
    if (it != _commands.end()) {
        (this->*(it->second))(client, params);
    } else {
        sendError(client, "421", cmd + " :Unknown command");
    }
}

void CommandHandler::parseAndHandleCommand(Client* client, const std::string& message) {
    std::vector<std::string> tokens = Utils::split(message, ' ');
    if (tokens.empty()) return;

    std::string command = tokens[0];
    tokens.erase(tokens.begin());
    handleCommand(client, command, tokens);
}

void CommandHandler::handlePass(Client* client, const std::vector<std::string>& params) {
    Utils::log("Handling PASS command");
    if (!validateParams(client, params, 1)) {
        Utils::log("PASS command: Invalid parameters");
        return;
    }

    if (client->isAuthenticated()) {
        Utils::log("PASS command: Client already authenticated");
        sendError(client, "462", ":You may not reregister");
        return;
    }

    Utils::log("PASS command: Checking password");
    if (params[0] == Server::getInstance().getPassword()) {
        Utils::log("PASS command: Password correct");
        client->setAuthenticated(true);
        sendReply(client, "001", ":Welcome to the IRC Network");
    } else {
        Utils::log("PASS command: Password incorrect");
        sendError(client, "464", ":Password incorrect");
    }
}

void CommandHandler::handleNick(Client* client, const std::vector<std::string>& params) {
    if (!validateParams(client, params, 1)) return;

    std::string newNick = params[0];
    if (!Utils::isValidNickname(newNick)) {
        sendError(client, "432", newNick + " :Erroneous nickname");
        return;
    }

    // Nickname'in kullanımda olup olmadığını kontrol et
    if (Server::getInstance().getClientByNickname(newNick)) {
        sendError(client, "433", newNick + " :Nickname is already in use");
        return;
    }

    client->setNickname(newNick);
    sendReply(client, "001", ":Welcome to the IRC Network " + newNick);
}

void CommandHandler::handleUser(Client* client, const std::vector<std::string>& params) {
    if (!validateParams(client, params, 4)) return;

    if (client->isRegistered()) {
        sendError(client, "462", ":You may not reregister");
        return;
    }

    std::string username = params[0];
    std::string realname = params[3];
    if (realname[0] == ':') {
        realname = realname.substr(1);
    }

    client->setUsername(username);
    client->setRealname(realname);
    client->setRegistered(true);
    
    // Kullanıcı kaydı tamamlandığında hoş geldin mesajları
    sendReply(client, "001", ":Welcome to the IRC Network " + client->getNickname());
    sendReply(client, "002", ":Your host is " + Server::getInstance().getServerName());
    sendReply(client, "003", ":This server was created " + Utils::getServerCreationTime());
    sendReply(client, "004", Server::getInstance().getServerName() + " 1.0");
}

void CommandHandler::handleJoin(Client* client, const std::vector<std::string>& params) {
    if (!validateParams(client, params, 1)) return;

    if (!client->isRegistered()) {
        sendError(client, "451", ":You have not registered");
        return;
    }

    std::string channelName = params[0];
    if (!Utils::isValidChannelName(channelName)) {
        sendError(client, "476", channelName + " :Invalid channel name");
        return;
    }

    Channel* channel = Server::getInstance().getChannel(channelName);
    if (!channel) {
        channel = new Channel(channelName);
        Server::getInstance().createChannel(channelName, client);
    }

    // Kanal modlarını kontrol et
    if (channel->isInviteOnly() && !channel->hasClient(client)) {
        sendError(client, "473", channelName + " :Cannot join channel (+i)");
        return;
    }

    if (!channel->getKey().empty() && (params.size() < 2 || params[1] != channel->getKey())) {
        sendError(client, "475", channelName + " :Cannot join channel (+k)");
        return;
    }

    if (channel->getUserLimit() > 0 && static_cast<int>(channel->getClientCount()) >= channel->getUserLimit()) {
        sendError(client, "471", channelName + " :Cannot join channel (+l)");
        return;
    }

    channel->addClient(client);
    if (channel->getClientCount() == 1) {
        channel->addOperator(client);
    }
}

void CommandHandler::handlePart(Client* client, const std::vector<std::string>& params) {
    if (!validateParams(client, params, 1)) return;

    std::string channelName = params[0];
    Channel* channel = Server::getInstance().getChannel(channelName);
    if (!channel) {
        sendError(client, "403", channelName + " :No such channel");
        return;
    }

    if (!channel->hasClient(client)) {
        sendError(client, "442", channelName + " :You're not on that channel");
        return;
    }

    channel->removeClient(client);
}

void CommandHandler::handlePrivmsg(Client* client, const std::vector<std::string>& params) {
    if (!validateParams(client, params, 2)) return;

    std::string target = params[0];
    std::string message = params[1];
    if (message[0] == ':') {
        message = message.substr(1);
    }

    if (target[0] == '#' || target[0] == '&') {
        // Kanal mesajı
        Channel* channel = Server::getInstance().getChannel(target);
        if (!channel) {
            sendError(client, "403", target + " :No such channel");
            return;
        }
        if (!channel->hasClient(client)) {
            sendError(client, "404", target + " :Cannot send to channel");
            return;
        }
        channel->broadcast(client->getPrefix() + " PRIVMSG " + target + " :" + message, client);
    } else {
        // Özel mesaj
        Client* targetClient = Server::getInstance().getClientByNickname(target);
        if (!targetClient) {
            sendError(client, "401", target + " :No such nick");
            return;
        }
        Server::getInstance().sendToClient(targetClient->getSocket(), 
            client->getPrefix() + " PRIVMSG " + target + " :" + message);
    }
}

void CommandHandler::handleNotice(Client* client, const std::vector<std::string>& params) {
    // NOTICE komutu PRIVMSG'ye benzer, ancak hata mesajı döndürmez
    if (params.size() < 2) return;

    std::string target = params[0];
    std::string message = params[1];
    if (message[0] == ':') {
        message = message.substr(1);
    }

    if (target[0] == '#' || target[0] == '&') {
        Channel* channel = Server::getInstance().getChannel(target);
        if (channel && channel->hasClient(client)) {
            channel->broadcast(client->getPrefix() + " NOTICE " + target + " :" + message, client);
        }
    } else {
        Client* targetClient = Server::getInstance().getClientByNickname(target);
        if (targetClient) {
            Server::getInstance().sendToClient(targetClient->getSocket(),
                client->getPrefix() + " NOTICE " + target + " :" + message);
        }
    }
}

void CommandHandler::handleKick(Client* client, const std::vector<std::string>& params) {
    if (!validateParams(client, params, 2)) return;

    std::string channelName = params[0];
    std::string targetNick = params[1];
    std::string reason = params.size() > 2 ? params[2] : client->getNickname();

    Channel* channel = Server::getInstance().getChannel(channelName);
    if (!channel) {
        sendError(client, "403", channelName + " :No such channel");
        return;
    }

    if (!channel->isOperator(client)) {
        sendError(client, "482", channelName + " :You're not channel operator");
        return;
    }

    Client* targetClient = Server::getInstance().getClientByNickname(targetNick);
    if (!targetClient || !channel->hasClient(targetClient)) {
        sendError(client, "441", targetNick + " " + channelName + " :They aren't on that channel");
        return;
    }

    channel->broadcast(client->getPrefix() + " KICK " + channelName + " " + targetNick + " :" + reason);
    channel->removeClient(targetClient);
}

void CommandHandler::handleInvite(Client* client, const std::vector<std::string>& params) {
    if (!validateParams(client, params, 2)) return;

    std::string targetNick = params[0];
    std::string channelName = params[1];

    Channel* channel = Server::getInstance().getChannel(channelName);
    if (!channel) {
        sendError(client, "403", channelName + " :No such channel");
        return;
    }

    if (!channel->hasClient(client)) {
        sendError(client, "442", channelName + " :You're not on that channel");
        return;
    }

    if (channel->isInviteOnly() && !channel->isOperator(client)) {
        sendError(client, "482", channelName + " :You're not channel operator");
        return;
    }

    Client* targetClient = Server::getInstance().getClientByNickname(targetNick);
    if (!targetClient) {
        sendError(client, "401", targetNick + " :No such nick");
        return;
    }

    if (channel->hasClient(targetClient)) {
        sendError(client, "443", targetNick + " " + channelName + " :is already on channel");
        return;
    }

    Server::getInstance().sendToClient(targetClient->getSocket(),
        client->getPrefix() + " INVITE " + targetNick + " :" + channelName);
    sendReply(client, "341", "Inviting " + targetNick + " to " + channelName);
}

void CommandHandler::handleTopic(Client* client, const std::vector<std::string>& params) {
    if (!validateParams(client, params, 1)) return;

    std::string channelName = params[0];
    Channel* channel = Server::getInstance().getChannel(channelName);
    if (!channel) {
        sendError(client, "403", channelName + " :No such channel");
        return;
    }

    if (!channel->hasClient(client)) {
        sendError(client, "442", channelName + " :You're not on that channel");
        return;
    }

    if (params.size() == 1) {
        // Topic'i göster
        if (channel->getTopic().empty()) {
            sendReply(client, "331", channelName + " :No topic is set");
        } else {
            sendReply(client, "332", channelName + " :" + channel->getTopic());
        }
    } else {
        // Topic'i değiştir
        if (channel->isTopicProtected() && !channel->isOperator(client)) {
            sendError(client, "482", channelName + " :You're not channel operator");
            return;
        }

        std::string newTopic = params[1];
        if (newTopic[0] == ':') {
            newTopic = newTopic.substr(1);
        }
        channel->setTopic(newTopic);
        channel->broadcast(client->getPrefix() + " TOPIC " + channelName + " :" + newTopic);
    }
}

void CommandHandler::handleMode(Client* client, const std::vector<std::string>& params) {
    if (!validateParams(client, params, 1)) return;

    std::string target = params[0];
    if (target[0] == '#' || target[0] == '&') {
        // Kanal modu
        Channel* channel = Server::getInstance().getChannel(target);
        if (!channel) {
            sendError(client, "403", target + " :No such channel");
            return;
        }

        if (params.size() == 1) {
            // Mevcut modları göster
            sendReply(client, "324", target + " " + channel->getModes());
            return;
        }

        if (!channel->isOperator(client)) {
            sendError(client, "482", target + " :You're not channel operator");
            return;
        }

        std::string modes = params[1];
        bool adding = true;
        for (size_t i = 0; i < modes.length(); i++) {
            if (modes[i] == '+') {
                adding = true;
            } else if (modes[i] == '-') {
                adding = false;
            } else {
                std::string param = (i + 1 < modes.length() && modes[i + 1] == ' ') ? params[2] : "";
                channel->setMode(modes[i], adding, param);
            }
        }
    } else {
        // Kullanıcı modu (şu an için desteklenmiyor)
        sendError(client, "501", ":Unknown MODE flag");
    }
}

void CommandHandler::handleQuit(Client* client, const std::vector<std::string>& params) {
    std::string message = params.empty() ? "Client Quit" : params[0];
    if (message[0] == ':') {
        message = message.substr(1);
    }

    Server::getInstance().removeClient(client->getSocket());
}

void CommandHandler::handleCap(Client* client, const std::vector<std::string>& params) {
    (void)client;
    (void)params;
    // CAP komutunu görmezden gel, modern IRC client'lar için gerekli
}

void CommandHandler::handlePing(Client* client, const std::vector<std::string>& params) {
    if (!validateParams(client, params, 1)) return;
    Server::getInstance().sendToClient(client->getSocket(), "PONG :" + params[0]);
}

void CommandHandler::handlePong(Client* client, const std::vector<std::string>& params) {
    (void)client;
    (void)params;
    // PONG yanıtları işlenmez
}

void CommandHandler::sendError(Client* client, const std::string& code, const std::string& message) {
    std::string nickname = client->getNickname().empty() ? "*" : client->getNickname();
    Server::getInstance().sendToClient(client->getSocket(), 
        ":" + Server::getInstance().getServerName() + " " + code + " " + nickname + " " + message);
}

void CommandHandler::sendReply(Client* client, const std::string& code, const std::string& message) {
    std::string nickname = client->getNickname().empty() ? "*" : client->getNickname();
    Server::getInstance().sendToClient(client->getSocket(),
        ":" + Server::getInstance().getServerName() + " " + code + " " + nickname + " " + message);
}

bool CommandHandler::validateParams(Client* client, const std::vector<std::string>& params, size_t required) {
    if (params.size() < required) {
        sendError(client, "461", ":Not enough parameters");
        return false;
    }
    return true;
} 
