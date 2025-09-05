#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include <iostream>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>

Server::Server(int port, const char *password)
    : _port(port), _password(std::string(password)), _listen_fd(-1)
{
}

Server::~Server()
{

    for (std::map<int, Client *>::iterator it = _clients.begin(); it != _clients.end(); ++it)
    {
        delete it->second;
    }
    _clients.clear();
    _clients_by_nick.clear();

    for (std::map<std::string, Channel *>::iterator it = _channels.begin(); it != _channels.end(); ++it)
    {
        delete it->second;
    }
    _channels.clear();

    if (_listen_fd >= 0)
        close(_listen_fd);
}

void Server::run()
{
    _listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_listen_fd < 0)
    {
        std::cerr << "Socket could not be created" << std::endl;
        return;
    }

    int opt = 1;
    setsockopt(_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    fcntl(_listen_fd, F_SETFL, O_NONBLOCK);

    sockaddr_in serv_addr;
    std::memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(_port);

    if (bind(_listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        std::cerr << "Bind error" << std::endl;
        close(_listen_fd);
        return;
    }

    if (listen(_listen_fd, 10) < 0)
    {
        std::cerr << "Listen error" << std::endl;
        close(_listen_fd);
        return;
    }

    std::cout << "IRC Server is running on port " << _port << std::endl;

    FD_ZERO(&_master_set);
    FD_SET(_listen_fd, &_master_set);
    _fd_max = _listen_fd;

    while (!g_stop)
    {
        _read_fds = _master_set;
        int activity = select(_fd_max + 1, &_read_fds, NULL, NULL, NULL);
        if (activity < 0)
        {
            std::cerr << "select() error" << std::endl;
            break;
        }

        for (int fd = 0; fd <= _fd_max; ++fd)
        {
            if (FD_ISSET(fd, &_read_fds))
            {
                if (fd == _listen_fd)
                {
                    handleNewConnection(fd);
                }
                else
                {
                    std::map<int, Client *>::iterator it = _clients.find(fd);
                    if (it != _clients.end())
                    {
                        handleClientData(it->second);
                    }
                }
            }
        }
    }
}
