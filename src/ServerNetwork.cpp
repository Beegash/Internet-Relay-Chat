#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

void Server::handleNewConnection(int listen_fd)
{
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd >= 0)
    {
        fcntl(client_fd, F_SETFL, O_NONBLOCK);

        Client *client = new Client(client_fd);
        _clients[client_fd] = client;

        FD_SET(client_fd, &_master_set);
        if (client_fd > _fd_max)
        {
            _fd_max = client_fd;
        }

        std::cout << "New connection: " << client_fd << " (fd_max: " << _fd_max << ")" << std::endl;

        client->sendMessage(":localhost NOTICE * :Please authenticate with PASS <password> before using other commands.\r\n");
    }
}

void Server::handleClientData(Client *client)
{
    char buf[512];
    int nbytes = recv(client->getFd(), buf, sizeof(buf) - 1, 0);
    if (nbytes <= 0)
    {
        std::cout << "Connection closed: " << client->getFd() << std::endl;
        removeClient(client);
    }
    else
    {
        buf[nbytes] = '\0';
        std::string data(buf);

        std::cout << "[" << client->getFd() << "] Received data: " << data << std::endl;

        std::string &buffer = client->_buffer;
        buffer += data;

        size_t pos;
        while ((pos = buffer.find("\r\n")) != std::string::npos)
        {
            std::string command = buffer.substr(0, pos);
            buffer.erase(0, pos + 2);

            if (!command.empty())
            {
                std::cout << "[" << client->getFd() << "] Processing command: " << command << std::endl;
                processCommand(client, command);
            }
        }
    }
}

void Server::removeClient(Client *client)
{
    if (!client)
        return;

    int fd = client->getFd();

    std::cout << "Removing client: " << fd << std::endl;

    FD_CLR(fd, &_master_set);

    if (fd == _fd_max)
    {
        _fd_max = _listen_fd;
        for (std::map<int, Client *>::iterator it = _clients.begin(); it != _clients.end(); ++it)
        {
            if (it->first != fd && it->first > _fd_max)
            {
                _fd_max = it->first;
            }
        }
    }

    if (!client->getNickname().empty())
    {
        _clients_by_nick.erase(client->getNickname());
    }

    std::vector<std::string> channelsToDelete;
    for (std::map<std::string, Channel *>::iterator it = _channels.begin(); it != _channels.end(); ++it)
    {
        Channel *channel = it->second;
        if (channel->hasClient(client))
        {
            bool wasOperator = channel->isOperator(client);
            channel->removeClient(client);

            if (wasOperator && !channel->hasOperators() && !channel->getClients().empty())
            {
                channel->promoteNextOperator();
            }

            if (channel->getClients().empty())
            {
                channelsToDelete.push_back(it->first);
            }
        }
    }

    for (std::vector<std::string>::iterator it = channelsToDelete.begin(); it != channelsToDelete.end(); ++it)
    {
        std::cout << "Deleting empty channel: " << *it << std::endl;
        delete _channels[*it];
        _channels.erase(*it);
    }

    _clients.erase(fd);
    delete client;
}
