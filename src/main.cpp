#include <iostream>
#include <cstdlib>
#include "Server.hpp"

volatile sig_atomic_t g_stop = 0;

void HandleSigint(int)
{
    g_stop = 1;
}

int main(int argc, char *argv[])
{
    signal(SIGINT, HandleSigint);
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <port> <password>" << std::endl;
        return 1;
    }
    if(argv[2][0] == '\0')
    {
        std::cerr << "Password cannot be empty." << std::endl;
        return 1;
    }
    for(size_t i = 0; argv[1][i]; i++)
    {
        if(!isdigit(argv[1][i]))
        {
            std::cerr << "Port must be a valid number." << std::endl;
            return 1;
        }
    }
    int port = std::atoi(argv[1]);
    if (port <= 0 || port > 65535)
    {
        std::cerr << "Invalid port number." << std::endl;
        return 1;
    }
    Server server(port, argv[2]);
    server.run();
    return 0;
}
