#include "Server.hpp"

#include "Client.hpp"
#include "Channel.hpp"
#include <iostream>
#include <stdlib.h>
#include <algorithm>
#include <sstream>

void Server::processCommand(Client *client, const std::string &command)
{
    std::vector<std::string> args = splitCommand(command);
    if (args.empty())
        return;

    std::string cmd = args[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

    // Şifre kontrolü: PASS hariç tüm kritik komutlarda uygula
    if (cmd != "PASS" && !_password.empty() && !client->isAuthenticated())
    {
        client->sendMessage(":localhost 464 * :Password required\r\n");
        return;
    }

    if (cmd == "PASS")
    {
        handlePass(client, args);
    }
    else if (cmd == "NICK")
    {
        handleNick(client, args);
    }
    else if (cmd == "USER")
    {
        handleUser(client, args);
    }
    else if (cmd == "JOIN")
    {
        handleJoin(client, args);
    }
    else if (cmd == "PART")
    {
        handlePart(client, args);
    }
    else if (cmd == "PRIVMSG")
    {
        handlePrivmsg(client, args);
    }
    else if (cmd == "KICK")
    {
        handleKick(client, args);
    }
    else if (cmd == "INVITE")
    {
        handleInvite(client, args);
    }
    else if (cmd == "TOPIC")
    {
        handleTopic(client, args);
    }
    else if (cmd == "MODE")
    {
        handleMode(client, args);
    }
    else if (cmd == "QUIT")
    {
        handleQuit(client, args);
    }
    else if (cmd == "CAP")
    {
        handleCap(client, args);
    }
    else if (cmd == "PING")
    {
        handlePing(client, args);
    }
    else if (cmd == "NOTICE")
    {
        handleNotice(client, args);
    }
    else if (cmd == "WHO")
    {
        handleWho(client, args);
    }
    else
    {
        // Unknown command
        client->sendMessage(":localhost 421 * " + cmd + " :Unknown command\r\n");
    }
}

// === AUTHENTICATION COMMANDS ===

void Server::handlePass(Client *client, const std::vector<std::string> &args)
{
    if (args.size() < 2)
    {
        client->sendMessage(":localhost 461 * PASS :Not enough parameters\r\n");
        return;
    }

    if (args[1] == _password)
    {
        client->setAuthenticated(true);
        client->sendMessage(":localhost 001 * :Şifre doğrulandı. Lütfen NICK ve USER komutlarını girin.\r\n");
    }
    else
    {
        client->sendMessage(":localhost 464 * :Password incorrect\r\n");
    }
}

void Server::handleNick(Client *client, const std::vector<std::string> &args)
{
    if (args.size() < 2)
    {
        client->sendMessage(":localhost 431 * :No nickname given\r\n");
        return;
    }

    std::string requestedNick = args[1];
    std::string nickname = requestedNick;

    // Check for nickname collision and alternative suggestion
    if (findClientByNickname(nickname))
    {
        // If client is not registered, suggest alternative nickname
        if (!client->isRegistered())
        {
            for (int i = 1; i <= 99; ++i)
            {
                std::ostringstream oss;
                oss << requestedNick << i;
                std::string altNick = oss.str();
                if (!findClientByNickname(altNick))
                {
                    nickname = altNick;
                    break;
                }
            }

            if (nickname == requestedNick)
            {
                // Still in collision, error
                client->sendMessage(":localhost 433 * " + requestedNick + " :Nickname is already in use\r\n");
                return;
            }
        }
        else
        {
            // Error for registered client
            client->sendMessage(":localhost 433 * " + requestedNick + " :Nickname is already in use\r\n");
            return;
        }
    }

    std::string oldNick = client->getNickname();

    // Remove old nickname
    if (!oldNick.empty())
    {
        _clients_by_nick.erase(oldNick);
    }

    client->setNickname(nickname);
    _clients_by_nick[nickname] = client;

    if (client->isRegistered() && !oldNick.empty())
    {
        // Notify user of NICK change
        std::string nickMsg = ":" + oldNick + "!user@localhost NICK :" + nickname + "\r\n";
        client->sendMessage(nickMsg);

        // Check if user is banned with new nickname in any channels they're in
        std::vector<Channel *> channelsToLeave;
        for (std::map<std::string, Channel *>::iterator it = _channels.begin(); it != _channels.end(); ++it)
        {
            Channel *channel = it->second;
            if (channel->hasClient(client))
            {
                // Check if new nickname is banned
                std::vector<std::string> banList = channel->getBanList();
                bool isBanned = false;
                for (std::vector<std::string>::iterator banIt = banList.begin(); banIt != banList.end(); ++banIt)
                {
                    std::string banMask = *banIt;
                    if (banMask == nickname || banMask == nickname + "!*@*")
                    {
                        isBanned = true;
                        break;
                    }
                }

                if (isBanned)
                {
                    // Mark channel for removal (can't remove during iteration)
                    channelsToLeave.push_back(channel);
                }
                else
                {
                    // Broadcast NICK change to this channel
                    channel->broadcast(nickMsg);
                }
            }
        }

        // Remove user from banned channels
        for (std::vector<Channel *>::iterator it = channelsToLeave.begin(); it != channelsToLeave.end(); ++it)
        {
            Channel *channel = *it;
            std::string kickMsg = ":localhost KICK " + channel->getName() + " " + nickname + " :Banned nickname\r\n";
            channel->broadcast(kickMsg);

            bool wasOperator = channel->isOperator(client);
            channel->removeClient(client);

            // If the user was an operator and channel is not empty, promote new operator
            if (wasOperator && !channel->getClients().empty())
            {
                channel->promoteNextOperator();
            }
        }
    }
    else if (client->isRegistered())
    {
        // First time setting nick for registered user
        std::string nickMsg = ":localhost NICK " + nickname + "\r\n";
        client->sendMessage(nickMsg);
    }

    // If client is not yet registered and we used alternative nick, notify
    if (!client->isRegistered() && nickname != requestedNick)
    {
        client->sendMessage(":localhost NOTICE * :Your requested nickname " + requestedNick + " is in use, using " + nickname + " instead\r\n");
    }
}

void Server::handleUser(Client *client, const std::vector<std::string> &args)
{
    if (args.size() < 5)
    {
        client->sendMessage(":localhost 461 * USER :Not enough parameters\r\n");
        return;
    }

    client->setUsername(args[1]);
    client->setRealname(args[4]);
    client->setRegistered(true);

    if (!client->getNickname().empty())
    {
        sendWelcome(client);
    }
}

// === CHANNEL COMMANDS ===

void Server::handleJoin(Client *client, const std::vector<std::string> &args)
{
    if (!client->isRegistered())
    {
        client->sendMessage(":localhost 451 * :You have not registered\r\n");
        return;
    }

    if (args.size() < 2)
    {
        client->sendMessage(":localhost 461 * JOIN :Not enough parameters\r\n");
        return;
    }

    std::string channelName = args[1];
    if (channelName[0] != '#')
    {
        client->sendMessage(":localhost 403 * " + channelName + " :Invalid channel name\r\n");
        return;
    }

    // Channel key check (args[2] optional)
    std::string key = (args.size() > 2) ? args[2] : "";

    Channel *channel = findChannel(channelName);
    if (!channel)
    {
        channel = createChannel(channelName);
        // First person to join becomes operator automatically
        channel->addOperator(client);
    }

    // Ban check - is user banned?
    std::string nickname = client->getNickname();
    std::vector<std::string> banList = channel->getBanList();
    for (std::vector<std::string>::iterator it = banList.begin(); it != banList.end(); ++it)
    {
        std::string banMask = *it;
        // Ban check: exact nickname or nickname with wildcards
        if (banMask == nickname || banMask == nickname + "!*@*")
        {
            client->sendMessage(":localhost 474 " + nickname + " " + channelName + " :Cannot join channel (+b)\r\n");
            return;
        }
    }

    // Check if channel key is set and matches
    if (!channel->getKey().empty() && channel->getKey() != key)
    {
        client->sendMessage(":localhost 475 * " + channelName + " :Cannot join channel (+k)\r\n");
        return;
    }

    // Invite-only (+i) kontrolü
    if (channel->isInviteOnly())
    {
        if (!channel->isInvited(nickname))
        {
            client->sendMessage(":localhost 473 " + nickname + " " + channelName + " :Cannot join channel (+i)\r\n");
            return;
        }
    }

    // User limit (+l) kontrolü
    if (channel->getUserLimit() > 0)
    {
        std::vector<Client *> clients = channel->getClients();
        if (clients.size() >= static_cast<size_t>(channel->getUserLimit()))
        {
            client->sendMessage(":localhost 471 " + nickname + " " + channelName + " :Cannot join channel (+l)\r\n");
            return;
        }
    }

    // Already in channel?
    if (channel->hasClient(client))
    {
        return; // Already in channel, do nothing
    }

    channel->addClient(client);

    // Send JOIN message to all clients in the channel (including self)
    std::string joinMsg = ":" + nickname + "!user@localhost JOIN " + channelName + "\r\n";
    channel->broadcast(joinMsg);

    // Send channel information only to the new client
    if (channel->getTopic().empty())
    {
        client->sendMessage(":localhost 331 " + nickname + " " + channelName + " :No topic is set\r\n");
    }
    else
    {
        client->sendMessage(":localhost 332 " + nickname + " " + channelName + " :" + channel->getTopic() + "\r\n");
    }

    // NAMES list - show all clients in the channel (only to the new client)
    std::string namesList = "";
    std::vector<Client *> clients = channel->getClients();
    for (std::vector<Client *>::iterator it = clients.begin(); it != clients.end(); ++it)
    {
        if (!namesList.empty())
            namesList += " ";
        std::string prefix = channel->isOperator(*it) ? "@" : "";
        namesList += prefix + (*it)->getNickname();
    }
    client->sendMessage(":localhost 353 " + nickname + " = " + channelName + " :" + namesList + "\r\n");
    client->sendMessage(":localhost 366 " + nickname + " " + channelName + " :End of /NAMES list\r\n");

    // Daveti tek kullanımlık tüket
    if (channel->isInvited(nickname))
    {
        channel->removeInvitation(nickname);
    }
}

void Server::handlePart(Client *client, const std::vector<std::string> &args)
{
    if (args.size() < 2)
    {
        client->sendMessage(":localhost 461 * PART :Not enough parameters\r\n");
        return;
    }

    std::string channelName = args[1];
    Channel *channel = findChannel(channelName);
    if (!channel)
    {
        client->sendMessage(":localhost 403 * " + channelName + " :No such channel\r\n");
        return;
    }

    if (!channel->hasClient(client))
    {
        client->sendMessage(":localhost 442 * " + channelName + " :You're not on that channel\r\n");
        return;
    }

    std::string nickname = client->getNickname();
    std::string reason = (args.size() > 2) ? args[2] : "";
    if (!reason.empty() && reason[0] == ':')
    {
        reason = reason.substr(1);
    }

    std::string partMsg = ":" + nickname + "!user@localhost PART " + channelName;
    if (!reason.empty())
    {
        partMsg += " :" + reason;
    }
    partMsg += "\r\n";

    channel->broadcast(partMsg);

    bool wasOperator = channel->isOperator(client);
    channel->removeClient(client);

    // If the operator left and the channel is not empty, select a new operator
    if (wasOperator && !channel->hasOperators() && !channel->getClients().empty())
    {
        channel->promoteNextOperator();
    }

    // Eğer kanal boşaldıysa sil
    if (channel->getClients().empty())
    {
        std::cout << "Channel " << channelName << " is empty, deleting..." << std::endl;
        _channels.erase(channelName);
        delete channel;
    }
}

void Server::handlePrivmsg(Client *client, const std::vector<std::string> &args)
{
    if (args.size() < 3)
    {
        client->sendMessage(":localhost 461 * PRIVMSG :Not enough parameters\r\n");
        return;
    }

    std::string target = args[1];
    std::string message = args[2];

    // If message starts with ":", remove it
    if (message[0] == ':')
    {
        message = message.substr(1);
    }

    if (target[0] == '#')
    {
        // Channel message
        Channel *channel = findChannel(target);
        if (!channel)
        {
            client->sendMessage(":localhost 403 * " + target + " :No such channel\r\n");
            return;
        }

        if (!channel->hasClient(client))
        {
            client->sendMessage(":localhost 404 * " + target + " :Cannot send to channel\r\n");
            return;
        }

        std::string nickname = client->getNickname();
        std::string privmsg = ":" + nickname + "!user@localhost PRIVMSG " + target + " :" + message + "\r\n";
        channel->broadcast(privmsg, client);
    }
    else
    {
        // Private message
        Client *targetClient = findClientByNickname(target);
        if (!targetClient)
        {
            // RFC 1459: 401 <nick> <target> :No such nick/channel
            client->sendMessage(":localhost 401 " + client->getNickname() + " " + target + " :No such nick/channel\r\n");
            return;
        }

        std::string nickname = client->getNickname();
        std::string privmsg = ":" + nickname + "!user@localhost PRIVMSG " + target + " :" + message + "\r\n";

        // Send message only to the receiver - sender will automatically see it in KVIrc
        targetClient->sendMessage(privmsg);
    }
}

void Server::handleKick(Client *client, const std::vector<std::string> &args)
{
    if (args.size() < 3)
    {
        client->sendMessage(":localhost 461 * KICK :Not enough parameters\r\n");
        return;
    }

    std::string channelName = args[1];
    std::string targetNick = args[2];
    std::string reason = (args.size() > 3) ? args[3] : client->getNickname();

    // If reason parameter starts with ":", remove it
    if (!reason.empty() && reason[0] == ':')
    {
        reason = reason.substr(1);
    }

    // Self-kick prevention
    if (targetNick == client->getNickname())
    {
        client->sendMessage(":localhost 484 * " + channelName + " :You can't kick yourself\r\n");
        return;
    }

    Channel *channel = findChannel(channelName);
    if (!channel)
    {
        client->sendMessage(":localhost 403 * " + channelName + " :No such channel\r\n");
        return;
    }

    if (!channel->isOperator(client))
    {
        client->sendMessage(":localhost 482 * " + channelName + " :You're not channel operator\r\n");
        return;
    }

    Client *targetClient = findClientByNickname(targetNick);
    if (!targetClient)
    {
        client->sendMessage(":localhost 401 * " + targetNick + " :No such nick/channel\r\n");
        return;
    }

    if (!channel->hasClient(targetClient))
    {
        client->sendMessage(":localhost 441 * " + targetNick + " " + channelName + " :They aren't on that channel\r\n");
        return;
    }

    std::string nickname = client->getNickname();
    std::string kickMsg = ":" + nickname + "!user@localhost KICK " + channelName + " " + targetNick + " :" + reason + "\r\n";
    channel->broadcast(kickMsg);

    bool wasOperator = channel->isOperator(targetClient);
    channel->removeClient(targetClient);

    // If the kicked operator left and the channel is not empty, select a new operator
    if (wasOperator && !channel->hasOperators() && !channel->getClients().empty())
    {
        channel->promoteNextOperator();
    }
}

void Server::handleInvite(Client *client, const std::vector<std::string> &args)
{
    if (args.size() < 3)
    {
        client->sendMessage(":localhost 461 * INVITE :Not enough parameters\r\n");
        return;
    }

    std::string targetNick = args[1];
    std::string channelName = args[2];

    Channel *channel = findChannel(channelName);
    if (!channel)
    {
        client->sendMessage(":localhost 403 * " + channelName + " :No such channel\r\n");
        return;
    }

    if (!channel->isOperator(client))
    {
        client->sendMessage(":localhost 482 * " + channelName + " :You're not channel operator\r\n");
        return;
    }

    Client *targetClient = findClientByNickname(targetNick);
    if (!targetClient)
    {
        client->sendMessage(":localhost 401 * " + targetNick + " :No such nick/channel\r\n");
        return;
    }

    if (channel->hasClient(targetClient))
    {
        client->sendMessage(":localhost 443 * " + targetNick + " " + channelName + " :is already on channel\r\n");
        return;
    }

    std::string nickname = client->getNickname();
    std::string inviteMsg = ":" + nickname + "!user@localhost INVITE " + targetNick + " " + channelName + "\r\n";
    targetClient->sendMessage(inviteMsg);
    client->sendMessage(":localhost 341 " + nickname + " " + targetNick + " " + channelName + "\r\n");
    channel->addInvitation(targetNick);
}

void Server::handleTopic(Client *client, const std::vector<std::string> &args)
{
    if (args.size() < 2)
    {
        client->sendMessage(":localhost 461 * TOPIC :Not enough parameters\r\n");
        return;
    }

    std::string channelName;
    Channel *channel;
    bool explicitChannel = (args[1][0] == '#');

    // TOPIC komutunun iki format'ını destekle:
    // 1. TOPIC selam (mevcut kanalda topic değiştir)
    // 2. TOPIC #sohbet1 selam (belirtilen kanalda topic değiştir)

    if (explicitChannel)
    {
        // Format 2: TOPIC #kanal topic
        channelName = args[1];
        channel = findChannel(channelName);
        if (!channel)
        {
            client->sendMessage(":localhost 403 * " + channelName + " :No such channel\r\n");
            return;
        }
    }
    else
    {
        if (args.size() >= 2)
        {
            channel = NULL;
            for (std::map<std::string, Channel *>::iterator it = _channels.begin(); it != _channels.end(); ++it)
            {
                if (it->second->hasClient(client))
                {
                    channel = it->second;
                    channelName = it->first;
                    break;
                }
            }

            if (!channel)
            {
                client->sendMessage(":localhost 442 * :You're not on any channel\r\n");
                return;
            }
        }
        else
        {
            client->sendMessage(":localhost 461 * TOPIC :Not enough parameters\r\n");
            return;
        }
    }

    if (!channel->hasClient(client))
    {
        client->sendMessage(":localhost 442 * " + channelName + " :You're not on that channel\r\n");
        return;
    }

    // Only show topic if explicitChannel && args.size() == 2
    if (explicitChannel && args.size() == 2)
    {
        // Show topic
        std::string nickname = client->getNickname();
        if (channel->getTopic().empty())
        {
            client->sendMessage(":localhost 331 " + nickname + " " + channelName + " :No topic is set\r\n");
        }
        else
        {
            client->sendMessage(":localhost 332 " + nickname + " " + channelName + " :" + channel->getTopic() + "\r\n");
        }
    }
    else
    {
        // Change topic
        // +t mode aktifken sadece operator'lar topic değiştirebilir
        // -t mode aktifken herkes topic değiştirebilir
        if (channel->isTopicRestricted() && !channel->isOperator(client))
        {
            client->sendMessage(":localhost 482 * " + channelName + " :You're not channel operator\r\n");
            return;
        }

        // Topic parametrelerini birleştir
        std::string topic;

        if (explicitChannel)
        {
            // Format 2: TOPIC #kanal topic (args[2] ve sonrası)
            for (size_t i = 2; i < args.size(); ++i)
            {
                std::string param = args[i];

                // Her parametrede ':' işareti varsa kaldır
                if (param[0] == ':')
                {
                    param = param.substr(1);
                }

                if (i == 2)
                {
                    // İlk parametre
                    topic = param;
                }
                else
                {
                    // Sonraki parametreler için boşluk ekle
                    topic += " " + param;
                }
            }
        }
        else
        {
            // Format 1: TOPIC topic (args[1] ve sonrası)
            for (size_t i = 1; i < args.size(); ++i)
            {
                std::string param = args[i];

                // Her parametrede ':' işareti varsa kaldır
                if (param[0] == ':')
                {
                    param = param.substr(1);
                }

                if (i == 1)
                {
                    // İlk parametre
                    topic = param;
                }
                else
                {
                    // Sonraki parametreler için boşluk ekle
                    topic += " " + param;
                }
            }
        }

        channel->setTopic(topic);
        std::string nickname = client->getNickname();
        std::string topicMsg = ":" + nickname + "!user@localhost TOPIC " + channelName + " :" + topic + "\r\n";
        channel->broadcast(topicMsg);
    }
}

void Server::handleMode(Client *client, const std::vector<std::string> &args)
{
    // --- Enhanced logic for inferring channel and supporting /mode [modes] and /mode (no args) ---
    Channel *inferredChannel = NULL;
    std::string inferredName;
    if (args.size() < 2)
    {
        // Try to infer current channel for the client
        for (std::map<std::string, Channel *>::iterator it = _channels.begin(); it != _channels.end(); ++it)
        {
            if (it->second->hasClient(client))
            {
                inferredChannel = it->second;
                inferredName = it->first;
                break;
            }
        }
        if (!inferredChannel)
        {
            client->sendMessage(":localhost 461 * MODE :Not enough parameters\r\n");
            return;
        }
        // Show mode info as if /mode #channel
        std::string nickname = client->getNickname();
        std::string modes = "+";
        std::string modeParams = "";
        std::ostringstream oss;
        oss << inferredChannel->getUserLimit();
        if (inferredChannel->isInviteOnly())
            modes += "i";
        if (inferredChannel->isTopicRestricted())
            modes += "t";
        if (!inferredChannel->getKey().empty())
        {
            modes += "k";
            modeParams += " " + inferredChannel->getKey();
        }
        if (inferredChannel->getUserLimit() > 0)
        {
            modes += "l";
            modeParams += " " + oss.str();
        }
        client->sendMessage(":localhost 324 " + nickname + " " + inferredName + " " + modes + modeParams + "\r\n");
        return;
    }

    std::string target = args[1];
    // If target does not start with '#' but starts with '+' or '-', treat as /mode +flags (in channel)
    if (target[0] != '#' && (target[0] == '+' || target[0] == '-'))
    {
        // Infer channel
        for (std::map<std::string, Channel *>::iterator it = _channels.begin(); it != _channels.end(); ++it)
        {
            if (it->second->hasClient(client))
            {
                inferredChannel = it->second;
                inferredName = it->first;
                break;
            }
        }
        if (!inferredChannel)
        {
            client->sendMessage(":localhost 461 * MODE :Not enough parameters\r\n");
            return;
        }
        // Use inferred channel and treat args[1] as modes, args[2...] as mode params
        std::string modes = target;
        Channel *channel = inferredChannel;
        std::string channelName = inferredName;
        if (!channel->isOperator(client))
        {
            client->sendMessage(":localhost 482 * " + channelName + " :You're not channel operator\r\n");
            return;
        }
        // Simple mode handling (i, t, k, o, l)
        bool setting = true; // Default to adding modes
        size_t paramArg = 2;
        for (size_t i = 0; i < modes.length(); ++i)
        {
            char mode = modes[i];
            if (mode == '+')
            {
                setting = true;
                continue;
            }
            else if (mode == '-')
            {
                setting = false;
                continue;
            }
            if (mode == 'b')
            {
                if (args.size() > paramArg)
                {
                    std::string banMask = args[paramArg];
                    if (setting)
                    {
                        std::string nickname = client->getNickname();
                        if (banMask == nickname || banMask == nickname + "!*@*")
                        {
                            client->sendMessage(":localhost 485 * " + channelName + " :You cannot ban yourself\r\n");
                            continue;
                        }
                        if (banMask == "*!*@localhost" || banMask == "*!*@*" || banMask == "*")
                        {
                            client->sendMessage(":localhost 486 * " + channelName + " :Ban mask too broad - would ban everyone\r\n");
                            continue;
                        }
                        channel->addBan(banMask);
                        std::string modeMsg = ":" + nickname + "!user@localhost MODE " + channelName + " +b " + banMask + "\r\n";
                        channel->broadcast(modeMsg);
                        Client *bannedClient = NULL;
                        if (banMask.find("!") == std::string::npos && banMask.find("*") == std::string::npos)
                        {
                            bannedClient = findClientByNickname(banMask);
                        }
                        else if (banMask.length() > 4 && banMask.substr(banMask.length() - 4) == "!*@*")
                        {
                            std::string nickOnly = banMask.substr(0, banMask.length() - 4);
                            bannedClient = findClientByNickname(nickOnly);
                        }
                        if (bannedClient && channel->hasClient(bannedClient))
                        {
                            std::string kickMsg = ":" + nickname + "!user@localhost KICK " + channelName + " " + banMask + " :Banned\r\n";
                            channel->broadcast(kickMsg);
                            bool wasOperator = channel->isOperator(bannedClient);
                            channel->removeClient(bannedClient);
                            if (wasOperator && !channel->getClients().empty())
                            {
                                channel->promoteNextOperator();
                            }
                        }
                    }
                    else
                    {
                        channel->removeBan(banMask);
                        std::string nickname = client->getNickname();
                        std::string modeMsg = ":" + nickname + "!user@localhost MODE " + channelName + " -b " + banMask + "\r\n";
                        channel->broadcast(modeMsg);
                    }
                }
            }
            else if (mode == 'i')
            {
                channel->setInviteOnly(setting);
                std::string nickname = client->getNickname();
                std::string modeMsg = ":" + nickname + "!user@localhost MODE " + channelName + (setting ? " +i\r\n" : " -i\r\n");
                channel->broadcast(modeMsg);
                std::cout << "[" << client->getFd() << "] MODE " << channelName << (setting ? " +i" : " -i") << " set by " << nickname << std::endl;
            }
            else if (mode == 't')
            {
                channel->setTopicRestricted(setting);
                std::string nickname = client->getNickname();
                std::string modeMsg = ":" + nickname + "!user@localhost MODE " + channelName + (setting ? " +t\r\n" : " -t\r\n");
                channel->broadcast(modeMsg);
                std::cout << "[" << client->getFd() << "] MODE " << channelName << (setting ? " +t" : " -t") << " set by " << nickname << std::endl;
            }
            else if (mode == 'k')
            {
                if (setting && args.size() > paramArg)
                {
                    channel->setKey(args[paramArg]);
                    std::string nickname = client->getNickname();
                    std::string modeMsg = ":" + nickname + "!user@localhost MODE " + channelName + " +k " + args[paramArg] + "\r\n";
                    channel->broadcast(modeMsg);
                    std::cout << "[" << client->getFd() << "] MODE " << channelName << " +k " << args[paramArg] << " set by " << nickname << std::endl;
                }
                else if (!setting)
                {
                    channel->setKey("");
                    std::string nickname = client->getNickname();
                    std::string modeMsg = ":" + nickname + "!user@localhost MODE " + channelName + " -k\r\n";
                    channel->broadcast(modeMsg);
                    std::cout << "[" << client->getFd() << "] MODE " << channelName << " -k set by " << nickname << std::endl;
                }
            }
            else if (mode == 'l')
            {
                if (setting && args.size() > paramArg)
                {
                    const std::string &limStr = args[paramArg];
                    char *endptr = NULL;
                    long val = strtol(limStr.c_str(), &endptr, 10);
                    if (limStr.empty() || *endptr != '\0' || val <= 0)
                    {
                        client->sendMessage(":localhost 461 * MODE :Invalid +l parameter (limit must be a positive integer)\r\n");
                    }
                    else
                    {
                        int limit = static_cast<int>(val);
                        channel->setUserLimit(limit);
                        std::string nickname = client->getNickname();
                        std::string modeMsg = ":" + nickname + "!user@localhost MODE " + channelName + " +l " + limStr + "\r\n";
                        channel->broadcast(modeMsg);
                        std::cout << "[" << client->getFd() << "] MODE " << channelName << " +l " << limStr << " set by " << nickname << std::endl;
                    }
                }
                else if (!setting)
                {
                    channel->setUserLimit(0);
                    std::string nickname = client->getNickname();
                    std::string modeMsg = ":" + nickname + "!user@localhost MODE " + channelName + " -l\r\n";
                    channel->broadcast(modeMsg);
                    std::cout << "[" << client->getFd() << "] MODE " << channelName << " -l set by " << nickname << std::endl;
                }
            }
            else if (mode == 'o')
            {
                if (args.size() > paramArg)
                {
                    std::string targetNick = args[paramArg];
                    Client *targetClient = findClientByNickname(targetNick);
                    if (targetClient && channel->hasClient(targetClient))
                    {
                        std::string nickname = client->getNickname();
                        if (setting)
                        {
                            channel->addOperator(targetClient);
                            std::string modeMsg = ":" + nickname + "!user@localhost MODE " + channelName + " +o " + targetNick + "\r\n";
                            channel->broadcast(modeMsg);
                        }
                        else
                        {
                            channel->removeOperator(targetClient);
                            std::string modeMsg = ":" + nickname + "!user@localhost MODE " + channelName + " -o " + targetNick + "\r\n";
                            channel->broadcast(modeMsg);
                        }
                    }
                }
            }
            // For each mode that uses a parameter, increase paramArg
            if (mode == 'b' || mode == 'k' || mode == 'l' || mode == 'o')
                ++paramArg;
        }
        return;
    }

    if (target[0] == '#')
    {
        // Channel mode
        Channel *channel = findChannel(target);
        if (!channel)
        {
            client->sendMessage(":localhost 403 * " + target + " :No such channel\r\n");
            return;
        }

        if (args.size() == 2)
        {
            // Request mode information
            std::string nickname = client->getNickname();
            std::string modes = "+";
            std::string modeParams = "";
            std::ostringstream oss;
            oss << channel->getUserLimit();

            if (channel->isInviteOnly())
                modes += "i";
            if (channel->isTopicRestricted())
                modes += "t";
            if (!channel->getKey().empty())
            {
                modes += "k";
                modeParams += " " + channel->getKey();
            }
            if (channel->getUserLimit() > 0)
            {
                modes += "l";
                modeParams += " " + oss.str();
            }

            client->sendMessage(":localhost 324 " + nickname + " " + target + " " + modes + modeParams + "\r\n");
            return;
        }

        if (args.size() == 3 && args[2] == "b")
        {
            // Request ban list
            std::string nickname = client->getNickname();
            std::vector<std::string> banList = channel->getBanList();
            for (std::vector<std::string>::iterator it = banList.begin(); it != banList.end(); ++it)
            {
                client->sendMessage(":localhost 367 " + nickname + " " + target + " " + *it + " localhost 0\r\n");
            }
            client->sendMessage(":localhost 368 " + nickname + " " + target + " :End of channel ban list\r\n");
            return;
        }
    }

    if (args.size() < 3)
    {
        return; // Mode query processed, no parameters needed
    }

    std::string modes = args[2];

    if (target[0] == '#')
    {
        // Channel mode
        Channel *channel = findChannel(target);
        if (!channel)
        {
            client->sendMessage(":localhost 403 * " + target + " :No such channel\r\n");
            return;
        }

        if (!channel->isOperator(client))
        {
            client->sendMessage(":localhost 482 * " + target + " :You're not channel operator\r\n");
            return;
        }

        // Simple mode handling (i, t, k, o, l)
        bool setting = true; // Default to adding modes

        for (size_t i = 0; i < modes.length(); ++i)
        {
            char mode = modes[i];

            if (mode == '+')
            {
                setting = true;
                continue;
            }
            else if (mode == '-')
            {
                setting = false;
                continue;
            }

            if (mode == 'b')
            {
                if (args.size() > 3)
                {
                    std::string banMask = args[3];
                    if (setting)
                    {
                        // Self-ban prevention
                        std::string nickname = client->getNickname();
                        if (banMask == nickname || banMask == nickname + "!*@*")
                        {
                            client->sendMessage(":localhost 485 * " + target + " :You cannot ban yourself\r\n");
                            continue;
                        }

                        // Prevent overly broad ban masks that would ban everyone
                        if (banMask == "*!*@localhost" || banMask == "*!*@*" || banMask == "*")
                        {
                            client->sendMessage(":localhost 486 * " + target + " :Ban mask too broad - would ban everyone\r\n");
                            continue;
                        }

                        channel->addBan(banMask);
                        std::string modeMsg = ":" + nickname + "!user@localhost MODE " + target + " +b " + banMask + "\r\n";
                        channel->broadcast(modeMsg);

                        // Check if the banned user is currently in the channel and kick them
                        // Only for specific nickname bans, not wildcards
                        Client *bannedClient = NULL;
                        if (banMask.find("!") == std::string::npos && banMask.find("*") == std::string::npos)
                        {
                            // Simple nickname ban
                            bannedClient = findClientByNickname(banMask);
                        }
                        else if (banMask.length() > 4 && banMask.substr(banMask.length() - 4) == "!*@*")
                        {
                            // nickname!*@* format
                            std::string nickOnly = banMask.substr(0, banMask.length() - 4);
                            bannedClient = findClientByNickname(nickOnly);
                        }

                        if (bannedClient && channel->hasClient(bannedClient))
                        {
                            // Send kick message
                            std::string kickMsg = ":" + nickname + "!user@localhost KICK " + target + " " + banMask + " :Banned\r\n";
                            channel->broadcast(kickMsg);

                            // Remove from channel
                            bool wasOperator = channel->isOperator(bannedClient);
                            channel->removeClient(bannedClient);

                            // If the banned user was an operator and channel is not empty, promote new operator
                            if (wasOperator && !channel->getClients().empty())
                            {
                                channel->promoteNextOperator();
                            }
                        }
                    }
                    else
                    {
                        channel->removeBan(banMask);
                        std::string nickname = client->getNickname();
                        std::string modeMsg = ":" + nickname + "!user@localhost MODE " + target + " -b " + banMask + "\r\n";
                        channel->broadcast(modeMsg);
                    }
                }
            }
            else if (mode == 'i')
            {
                channel->setInviteOnly(setting);
                std::string nickname = client->getNickname();
                std::string modeMsg = ":" + nickname + "!user@localhost MODE " + target + (setting ? " +i\r\n" : " -i\r\n");
                channel->broadcast(modeMsg);

                // KVIrc için mode değişikliğini log'da göster
                std::cout << "[" << client->getFd() << "] MODE " << target << (setting ? " +i" : " -i") << " set by " << nickname << std::endl;
            }
            else if (mode == 't')
            {
                channel->setTopicRestricted(setting);
                std::string nickname = client->getNickname();
                std::string modeMsg = ":" + nickname + "!user@localhost MODE " + target + (setting ? " +t\r\n" : " -t\r\n");
                channel->broadcast(modeMsg);

                // KVIrc için mode değişikliğini log'da göster
                std::cout << "[" << client->getFd() << "] MODE " << target << (setting ? " +t" : " -t") << " set by " << nickname << std::endl;
            }
            else if (mode == 'k')
            {
                if (setting && args.size() > 3)
                {
                    channel->setKey(args[3]);
                    std::string nickname = client->getNickname();
                    // KVIrc'de +k mode'unun görünmesi için şifreyi broadcast ediyoruz
                    std::string modeMsg = ":" + nickname + "!user@localhost MODE " + target + " +k " + args[3] + "\r\n";
                    channel->broadcast(modeMsg);

                    // KVIrc için mode değişikliğini log'da göster
                    std::cout << "[" << client->getFd() << "] MODE " << target << " +k " << args[3] << " set by " << nickname << std::endl;
                }
                else if (!setting)
                {
                    channel->setKey("");
                    std::string nickname = client->getNickname();
                    std::string modeMsg = ":" + nickname + "!user@localhost MODE " + target + " -k\r\n";
                    channel->broadcast(modeMsg);

                    // KVIrc için mode değişikliğini log'da göster
                    std::cout << "[" << client->getFd() << "] MODE " << target << " -k set by " << nickname << std::endl;
                }
            }
            else if (mode == 'l')
            {
                if (setting && args.size() > 3)
                {
                    const std::string &limStr = args[3];
                    char *endptr = NULL;
                    long val = strtol(limStr.c_str(), &endptr, 10);
                    if (limStr.empty() || *endptr != '\0' || val <= 0)
                    {
                        client->sendMessage(":localhost 461 * MODE :Invalid +l parameter (limit must be a positive integer)\r\n");
                    }
                    else
                    {
                        int limit = static_cast<int>(val);
                        channel->setUserLimit(limit);
                        std::string nickname = client->getNickname();
                        std::string modeMsg = ":" + nickname + "!user@localhost MODE " + target + " +l " + limStr + "\r\n";
                        channel->broadcast(modeMsg);

                        // KVIrc için mode değişikliğini log'da göster
                        std::cout << "[" << client->getFd() << "] MODE " << target << " +l " << limStr << " set by " << nickname << std::endl;
                    }
                }
                else if (!setting)
                {
                    channel->setUserLimit(0);
                    std::string nickname = client->getNickname();
                    std::string modeMsg = ":" + nickname + "!user@localhost MODE " + target + " -l\r\n";
                    channel->broadcast(modeMsg);

                    // KVIrc için mode değişikliğini log'da göster
                    std::cout << "[" << client->getFd() << "] MODE " << target << " -l set by " << nickname << std::endl;
                }
            }
            else if (mode == 'o')
            {
                if (args.size() > 3)
                {
                    std::string targetNick = args[3];
                    Client *targetClient = findClientByNickname(targetNick);
                    if (targetClient && channel->hasClient(targetClient))
                    {
                        std::string nickname = client->getNickname();
                        if (setting)
                        {
                            channel->addOperator(targetClient);
                            std::string modeMsg = ":" + nickname + "!user@localhost MODE " + target + " +o " + targetNick + "\r\n";
                            channel->broadcast(modeMsg);
                        }
                        else
                        {
                            channel->removeOperator(targetClient);
                            std::string modeMsg = ":" + nickname + "!user@localhost MODE " + target + " -o " + targetNick + "\r\n";
                            channel->broadcast(modeMsg);
                        }
                    }
                }
            }
        }
    }
}

// === UTILITY COMMANDS ===

void Server::handleQuit(Client *client, const std::vector<std::string> &args)
{
    std::string message = args.size() > 1 ? args[1] : "Leaving";
    std::string nickname = client->getNickname();

    if (!nickname.empty())
    {
        std::string quitMsg = ":" + nickname + "!user@localhost QUIT :" + message + "\r\n";

        // Notify all clients in all channels
        for (std::map<std::string, Channel *>::iterator it = _channels.begin(); it != _channels.end(); ++it)
        {
            if (it->second->hasClient(client))
            {
                it->second->broadcast(quitMsg);
            }
        }
    }

    removeClient(client);
}

void Server::handleCap(Client *client, const std::vector<std::string> &args)
{
    if (args.size() < 2)
        return;

    std::string subcommand = args[1];
    std::transform(subcommand.begin(), subcommand.end(), subcommand.begin(), ::toupper);

    if (subcommand == "LS")
    {
        // CAP LS response - supported capabilities (empty)
        client->sendMessage(":localhost CAP * LS :\r\n");
        client->sendMessage(":localhost CAP * END\r\n");
    }
    else if (subcommand == "END")
    {
        // CAP negotiation completed
        // No special response needed
    }
    else if (subcommand == "REQ")
    {
        // Capability request - for now, reject all
        if (args.size() > 2)
        {
            client->sendMessage(":localhost CAP * NAK :" + args[2] + "\r\n");
        }
    }
}

void Server::handlePing(Client *client, const std::vector<std::string> &args)
{
    if (args.size() < 2)
    {
        client->sendMessage(":localhost 461 * PING :Not enough parameters\r\n");
        return;
    }

    // Respond to PING with PONG
    std::string target = args[1];
    client->sendMessage(":localhost PONG localhost :" + target + "\r\n");
}

void Server::handleNotice(Client *client, const std::vector<std::string> &args)
{
    // NOTICE commands usually don't require a response
    // For KVIrc's LAGCHECK messages, only log
    if (args.size() >= 3)
    {
        std::cout << "[" << client->getFd() << "] NOTICE: " << args[1] << " -> " << args[2] << std::endl;
    }
}

void Server::handleWho(Client *client, const std::vector<std::string> &args)
{
    if (args.size() < 2)
    {
        client->sendMessage(":localhost 461 * WHO :Not enough parameters\r\n");
        return;
    }

    std::string target = args[1];
    std::string nickname = client->getNickname();

    if (target[0] == '#')
    {
        // Channel WHO
        Channel *channel = findChannel(target);
        if (!channel)
        {
            client->sendMessage(":localhost 403 * " + target + " :No such channel\r\n");
            return;
        }

        if (!channel->hasClient(client))
        {
            client->sendMessage(":localhost 442 * " + target + " :You're not on that channel\r\n");
            return;
        }

        // List all clients in the channel
        std::vector<Client *> clients = channel->getClients();
        for (std::vector<Client *>::iterator it = clients.begin(); it != clients.end(); ++it)
        {
            std::string targetNick = (*it)->getNickname();
            std::string username = (*it)->getUsername();
            std::string realname = (*it)->getRealname();

            // If username/realname is empty, use defaults
            if (username.empty())
                username = "user";
            if (realname.empty())
                realname = targetNick;

            std::string prefix = channel->isOperator(*it) ? "@" : "";
            std::string flags = "H" + prefix; // H = Here (not away)

            // RFC 1459 WHO reply format: 352 <nick> <channel> <user> <host> <server> <nick> <flags> :<hopcount> <real name>
            client->sendMessage(":localhost 352 " + nickname + " " + target + " " + username + " localhost localhost " + targetNick + " " + flags + " :0 " + realname + "\r\n");
            // std::cout << "[WHO] Sent: :localhost 352 " << nickname << " " << target << " " << username << " localhost localhost " << targetNick << " " << flags << " :0 " << realname << std::endl;
        }
        client->sendMessage(":localhost 315 " + nickname + " " + target + " :End of /WHO list\r\n");
        // std::cout << "[WHO] Sent: :localhost 315 " << nickname << " " << target << " :End of /WHO list" << std::endl;
    }
    else
    {
        // Single user WHO
        Client *targetClient = findClientByNickname(target);
        if (!targetClient)
        {
            client->sendMessage(":localhost 401 * " + target + " :No such nick/channel\r\n");
            return;
        }

        std::string targetNick = targetClient->getNickname();
        std::string username = targetClient->getUsername();
        std::string realname = targetClient->getRealname();

        // If username/realname is empty, use defaults
        if (username.empty())
            username = "user";
        if (realname.empty())
            realname = targetNick;

        // Single user WHO reply
        client->sendMessage(":localhost 352 " + nickname + " * " + username + " localhost localhost " + targetNick + " H :0 " + realname + "\r\n");
        std::cout << "[WHO] Sent: :localhost 352 " << nickname << " * " << username << " localhost localhost " << targetNick << " H :0 " << realname << std::endl;
        client->sendMessage(":localhost 315 " + nickname + " " + target + " :End of /WHO list\r\n");
        std::cout << "[WHO] Sent: :localhost 315 " << nickname << " " << target << " :End of /WHO list" << std::endl;
    }
}
