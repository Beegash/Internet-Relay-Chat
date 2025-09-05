#include "Server.hpp"

#include "Client.hpp"
#include "Channel.hpp"
#include <iostream>
#include <stdlib.h>
#include <algorithm>
#include <sstream>

static bool isValidNick(const std::string &n)
{
    if (n.empty() || n.size() > 9)
        return false;

    const std::string firstAllowed = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz[]\\`^{}_|-";
    const std::string restAllowed  = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789[]\\`^{}_|-";

    if (firstAllowed.find(n[0]) == std::string::npos)
        return false;

    for (size_t i = 1; i < n.size(); ++i)
    {
        if (restAllowed.find(n[i]) == std::string::npos)
            return false;
    }
    return true;
}

void Server::processCommand(Client *client, const std::string &command)
{
    std::vector<std::string> args = splitCommand(command);
    if (args.empty())
        return;

    std::string cmd = args[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

    
    
    if (cmd != "PASS" && cmd != "CAP" && cmd != "PING" && cmd != "NOTICE" && cmd != "QUIT"
        && !_password.empty() && !client->isAuthenticated())
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
        
        client->sendMessage(":localhost 421 * " + cmd + " :Unknown command\r\n");
    }
}



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
        
        client->sendMessage(":localhost NOTICE * :Password accepted. Please provide NICK and USER.\r\n");
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

    
    if (!isValidNick(requestedNick))
    {
        std::string who = client->getNickname().empty() ? "*" : client->getNickname();
        client->sendMessage(":localhost 432 " + who + " " + requestedNick + " :Erroneous nickname\r\n");
        return;
    }

    
    if (findClientByNickname(nickname))
    {
        
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
                
                client->sendMessage(":localhost 433 * " + requestedNick + " :Nickname is already in use\r\n");
                return;
            }
        }
        else
        {
            
            client->sendMessage(":localhost 433 * " + requestedNick + " :Nickname is already in use\r\n");
            return;
        }
    }

    std::string oldNick = client->getNickname();

    
    if (!oldNick.empty())
    {
        _clients_by_nick.erase(oldNick);
    }

    client->setNickname(nickname);
    _clients_by_nick[nickname] = client;

    if (client->isRegistered() && !oldNick.empty())
    {
        
        std::string nickMsg = ":" + oldNick + "!user@localhost NICK :" + nickname + "\r\n";
        client->sendMessage(nickMsg);

        
        std::vector<Channel *> channelsToLeave;
        for (std::map<std::string, Channel *>::iterator it = _channels.begin(); it != _channels.end(); ++it)
        {
            Channel *channel = it->second;
            if (channel->hasClient(client))
            {
                
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
                    
                    channelsToLeave.push_back(channel);
                }
                else
                {
                    
                    channel->broadcast(nickMsg);
                }
            }
        }

        
        for (std::vector<Channel *>::iterator it = channelsToLeave.begin(); it != channelsToLeave.end(); ++it)
        {
            Channel *channel = *it;
            std::string kickMsg = ":localhost KICK " + channel->getName() + " " + nickname + " :Banned nickname\r\n";
            channel->broadcast(kickMsg);

            bool wasOperator = channel->isOperator(client);
            channel->removeClient(client);

            
            if (wasOperator && !channel->getClients().empty())
            {
                channel->promoteNextOperator();
            }
        }
    }
    else if (client->isRegistered())
    {
        
        
        sendWelcome(client);
    }

    
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

    
    std::string key = (args.size() > 2) ? args[2] : "";

    Channel *channel = findChannel(channelName);
    if (!channel)
    {
        channel = createChannel(channelName);
        
        channel->addOperator(client);
    }

    
    std::string nickname = client->getNickname();
    std::vector<std::string> banList = channel->getBanList();
    for (std::vector<std::string>::iterator it = banList.begin(); it != banList.end(); ++it)
    {
        std::string banMask = *it;
        
        if (banMask == nickname || banMask == nickname + "!*@*")
        {
            client->sendMessage(":localhost 474 " + nickname + " " + channelName + " :Cannot join channel (+b)\r\n");
            return;
        }
    }

    
    if (!channel->getKey().empty() && channel->getKey() != key)
    {
        client->sendMessage(":localhost 475 * " + channelName + " :Cannot join channel (+k)\r\n");
        return;
    }

    
    if (channel->isInviteOnly())
    {
        if (!channel->isInvited(nickname))
        {
            client->sendMessage(":localhost 473 " + nickname + " " + channelName + " :Cannot join channel (+i)\r\n");
            return;
        }
    }

    
    if (channel->getUserLimit() > 0)
    {
        std::vector<Client *> clients = channel->getClients();
        if (clients.size() >= static_cast<size_t>(channel->getUserLimit()))
        {
            client->sendMessage(":localhost 471 " + nickname + " " + channelName + " :Cannot join channel (+l)\r\n");
            return;
        }
    }

    
    if (channel->hasClient(client))
    {
        return; 
    }

    channel->addClient(client);

    
    std::string joinMsg = ":" + nickname + "!user@localhost JOIN " + channelName + "\r\n";
    channel->broadcast(joinMsg);

    
    if (channel->getTopic().empty())
    {
        client->sendMessage(":localhost 331 " + nickname + " " + channelName + " :No topic is set\r\n");
    }
    else
    {
        client->sendMessage(":localhost 332 " + nickname + " " + channelName + " :" + channel->getTopic() + "\r\n");
    }

    
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

    
    if (wasOperator && !channel->hasOperators() && !channel->getClients().empty())
    {
        channel->promoteNextOperator();
    }

    
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

    
    std::string message;
    if (args.size() >= 3)
    {
        
        for (size_t i = 2; i < args.size(); ++i)
        {
            std::string part = args[i];
            if (i == 2 && !part.empty() && part[0] == ':')
                part = part.substr(1);
            if (!message.empty()) message += " ";
            message += part;
        }
    }

    
    if (message.empty())
    {
        client->sendMessage(":localhost 412 * :No text to send\r\n");
        return;
    }

    if (target[0] == '#')
    {
        
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
        
        Client *targetClient = findClientByNickname(target);
        if (!targetClient)
        {
            
            client->sendMessage(":localhost 401 " + client->getNickname() + " " + target + " :No such nick/channel\r\n");
            return;
        }

        std::string nickname = client->getNickname();
        std::string privmsg = ":" + nickname + "!user@localhost PRIVMSG " + target + " :" + message + "\r\n";

        
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

    
    if (!reason.empty() && reason[0] == ':')
    {
        reason = reason.substr(1);
    }

    
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

    
    
    

    if (explicitChannel)
    {
        
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
        
        
        std::vector<Channel*> joined;
        for (std::map<std::string, Channel *>::iterator it = _channels.begin(); it != _channels.end(); ++it)
        {
            if (it->second->hasClient(client))
                joined.push_back(it->second);
        }

        if (joined.empty())
        {
            client->sendMessage(":localhost 442 * :You're not on any channel\r\n");
            return;
        }
        if (joined.size() > 1)
        {
            
            client->sendMessage(":localhost 461 * TOPIC :Channel name required (use: TOPIC #channel [<topic>])\r\n");
            return;
        }

        channel = joined[0];
        channelName = channel->getName();
    }

    if (!channel->hasClient(client))
    {
        client->sendMessage(":localhost 442 * " + channelName + " :You're not on that channel\r\n");
        return;
    }

    
    if (explicitChannel && args.size() == 2)
    {
        
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
        
        std::string nickname_for_err = client->getNickname();
        
        
        if (channel->isTopicRestricted() && !channel->isOperator(client))
        {
            client->sendMessage(":localhost 482 " + nickname_for_err + " " + channelName + " :You're not channel operator\r\n");
            return;
        }

        
        std::string topic;

        if (explicitChannel)
        {
            
            for (size_t i = 2; i < args.size(); ++i)
            {
                std::string param = args[i];

                
                if (param[0] == ':')
                {
                    param = param.substr(1);
                }

                if (i == 2)
                {
                    
                    topic = param;
                }
                else
                {
                    
                    topic += " " + param;
                }
            }
        }
        else
        {
            
            for (size_t i = 1; i < args.size(); ++i)
            {
                std::string param = args[i];

                
                if (param[0] == ':')
                {
                    param = param.substr(1);
                }

                if (i == 1)
                {
                    
                    topic = param;
                }
                else
                {
                    
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
    
    if (args.size() < 2)
    {
        client->sendMessage(":localhost 461 * MODE :Not enough parameters\r\n");
        return;
    }

    std::string target = args[1];

    
    if (target.empty() || target[0] != '#')
    {
        client->sendMessage(":localhost 461 * MODE :Channel name required (use: MODE #channel +/-modes)\r\n");
        return;
    }

    if (target[0] == '#')
    {
        
        Channel *channel = findChannel(target);
        if (!channel)
        {
            client->sendMessage(":localhost 403 * " + target + " :No such channel\r\n");
            return;
        }

        if (args.size() == 2)
        {
            
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
                
                if (channel->hasClient(client))
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
        return; 
    }

    std::string modes = args[2];

    if (target[0] == '#')
    {
        
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

        
        bool setting = true; 

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
                        
                        std::string nickname = client->getNickname();
                        if (banMask == nickname || banMask == nickname + "!*@*")
                        {
                            client->sendMessage(":localhost 485 * " + target + " :You cannot ban yourself\r\n");
                            continue;
                        }

                        
                        if (banMask == "*!*@localhost" || banMask == "*!*@*" || banMask == "*")
                        {
                            client->sendMessage(":localhost 486 * " + target + " :Ban mask too broad - would ban everyone\r\n");
                            continue;
                        }

                        channel->addBan(banMask);
                        std::string modeMsg = ":" + nickname + "!user@localhost MODE " + target + " +b " + banMask + "\r\n";
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
                            
                            std::string kickMsg = ":" + nickname + "!user@localhost KICK " + target + " " + banMask + " :Banned\r\n";
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

                
                std::cout << "[" << client->getFd() << "] MODE " << target << (setting ? " +i" : " -i") << " set by " << nickname << std::endl;
            }
            else if (mode == 't')
            {
                channel->setTopicRestricted(setting);
                std::string nickname = client->getNickname();
                std::string modeMsg = ":" + nickname + "!user@localhost MODE " + target + (setting ? " +t\r\n" : " -t\r\n");
                channel->broadcast(modeMsg);

                
                std::cout << "[" << client->getFd() << "] MODE " << target << (setting ? " +t" : " -t") << " set by " << nickname << std::endl;
            }
            else if (mode == 'k')
            {
                std::string nickname = client->getNickname();
                if (setting)
                {
                    if (args.size() <= 3 || args[3].empty())
                    {
                        client->sendMessage(":localhost 461 * MODE :Not enough parameters\r\n");
                        continue;
                    }
                    const std::string &newKey = args[3];
                    channel->setKey(newKey);
                    
                    std::string modeMsg = ":" + nickname + "!user@localhost MODE " + target + " +k " + newKey + "\r\n";
                    channel->broadcast(modeMsg);
                    std::cout << "[" << client->getFd() << "] MODE " << target << " +k " << newKey << " set by " << nickname << std::endl;
                }
                else
                {
                    
                    if (args.size() <= 3)
                    {
                        client->sendMessage(":localhost 461 * MODE :Not enough parameters\r\n");
                        continue;
                    }
                    const std::string &provided = args[3];
                    if (provided != channel->getKey())
                    {
                        
                        client->sendMessage(":localhost 525 * " + target + " :Key mismatch for -k\r\n");
                        continue;
                    }
                    channel->setKey("");
                    std::string modeMsg = ":" + nickname + "!user@localhost MODE " + target + " -k \r\n";
                    channel->broadcast(modeMsg);
                    std::cout << "[" << client->getFd() << "] MODE " << target << " -k set by " << nickname << std::endl;
                }
            }
            else if (mode == 'l')
            {
                if (setting)
                {
                    if (args.size() <= 3)
                    {
                        
                        client->sendMessage(":localhost 461 * MODE :Not enough parameters\r\n");
                    }
                    else
                    {
                        const std::string &limStr = args[3];
                        char *endptr = NULL;
                        long val = strtol(limStr.c_str(), &endptr, 10);
                        if (limStr.empty() || *endptr != '\0' || val <= 0)
                        {
                            client->sendMessage(":localhost 461 * MODE :Invalid +l parameter (limit must be a positive integer > 0)\r\n");
                        }
                        else
                        {
                            int limit = static_cast<int>(val);
                            channel->setUserLimit(limit);
                            std::string nickname = client->getNickname();
                            std::string modeMsg = ":" + nickname + "!user@localhost MODE " + target + " +l " + limStr + "\r\n";
                            channel->broadcast(modeMsg);

                            
                            std::cout << "[" << client->getFd() << "] MODE " << target << " +l " << limStr << " set by " << nickname << std::endl;
                        }
                    }
                }
                else
                {
                    
                    channel->setUserLimit(0);
                    std::string nickname = client->getNickname();
                    std::string modeMsg = ":" + nickname + "!user@localhost MODE " + target + " -l\r\n";
                    channel->broadcast(modeMsg);

                    
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



void Server::handleQuit(Client *client, const std::vector<std::string> &args)
{
    std::string message = args.size() > 1 ? args[1] : "Leaving";
    
    if (!message.empty() && message[0] == ':')
        message = message.substr(1);
    std::string nickname = client->getNickname();

    if (!nickname.empty())
    {
        std::string quitMsg = ":" + nickname + "!user@localhost QUIT :" + message + "\r\n";

        
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
        
        client->sendMessage(":localhost CAP * LS :\r\n");
        client->sendMessage(":localhost CAP * END\r\n");
    }
    else if (subcommand == "END")
    {
        
        
    }
    else if (subcommand == "REQ")
    {
        
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

    
    std::string target = args[1];
    client->sendMessage(":localhost PONG localhost :" + target + "\r\n");
}

void Server::handleNotice(Client *client, const std::vector<std::string> &args)
{
    
    
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

        
        std::vector<Client *> clients = channel->getClients();
        for (std::vector<Client *>::iterator it = clients.begin(); it != clients.end(); ++it)
        {
            std::string targetNick = (*it)->getNickname();
            std::string username = (*it)->getUsername();
            std::string realname = (*it)->getRealname();

            
            if (username.empty())
                username = "user";
            if (realname.empty())
                realname = targetNick;

            std::string prefix = channel->isOperator(*it) ? "@" : "";
            std::string flags = "H" + prefix; 

            
            client->sendMessage(":localhost 352 " + nickname + " " + target + " " + username + " localhost localhost " + targetNick + " " + flags + " :0 " + realname + "\r\n");
            
        }
        client->sendMessage(":localhost 315 " + nickname + " " + target + " :End of /WHO list\r\n");
        
    }
    else
    {
        
        Client *targetClient = findClientByNickname(target);
        if (!targetClient)
        {
            client->sendMessage(":localhost 401 * " + target + " :No such nick/channel\r\n");
            return;
        }

        std::string targetNick = targetClient->getNickname();
        std::string username = targetClient->getUsername();
        std::string realname = targetClient->getRealname();

        
        if (username.empty())
            username = "user";
        if (realname.empty())
            realname = targetNick;

        
        client->sendMessage(":localhost 352 " + nickname + " * " + username + " localhost localhost " + targetNick + " H :0 " + realname + "\r\n");
        std::cout << "[WHO] Sent: :localhost 352 " << nickname << " * " << username << " localhost localhost " << targetNick << " H :0 " << realname << std::endl;
        client->sendMessage(":localhost 315 " + nickname + " " + target + " :End of /WHO list\r\n");
        std::cout << "[WHO] Sent: :localhost 315 " << nickname << " " << target << " :End of /WHO list" << std::endl;
    }
}
