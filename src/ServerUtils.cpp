#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>

std::vector<std::string> Server::splitCommand(const std::string &command)
{
    std::vector<std::string> args;
    size_t start = 0;
    size_t pos = 0;

    while (pos < command.length())
    {
        if (command[pos] == ' ')
        {
            if (pos > start)
            {
                std::string arg = command.substr(start, pos - start);
                if (!arg.empty())
                {
                    args.push_back(arg);
                }
            }
            start = pos + 1;

            if (start < command.length() && command[start] == ':')
            {

                std::string trailing = command.substr(start);
                args.push_back(trailing);
                break;
            }
        }
        pos++;
    }

    if (start < command.length() && (start == 0 || command[start] != ':'))
    {
        std::string arg = command.substr(start);
        if (!arg.empty())
        {
            args.push_back(arg);
        }
    }

    return args;
}

Client *Server::findClientByNickname(const std::string &nickname)
{
    std::map<std::string, Client *>::iterator it = _clients_by_nick.find(nickname);
    return (it != _clients_by_nick.end()) ? it->second : NULL;
}

Channel *Server::findChannel(const std::string &name)
{
    std::map<std::string, Channel *>::iterator it = _channels.find(name);
    return (it != _channels.end()) ? it->second : NULL;
}

Channel *Server::createChannel(const std::string &name)
{
    Channel *channel = new Channel(name);
    _channels[name] = channel;
    return channel;
}

void Server::sendWelcome(Client *client)
{
    std::string nickname = client->getNickname();
    client->sendMessage(":localhost 001 " + nickname + " :Welcome to the Internet Relay Network " + nickname + "!user@localhost\r\n");
    client->sendMessage(":localhost 002 " + nickname + " :Your host is localhost, running version 1.0\r\n");
    client->sendMessage(":localhost 003 " + nickname + " :This server was created today\r\n");
    client->sendMessage(":localhost 004 " + nickname + " localhost 1.0 oiws biklmnopstv\r\n");
    client->sendMessage(":localhost 005 " + nickname + " CHANTYPES=# PREFIX=(ov)@+ NETWORK=LocalIRC :are supported by this server\r\n");
}
