#include "server.hpp"
#include <sys/socket.h>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>

int init_network(Server* server, int port)
{
    server->server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->server_socket_fd < 0) {
        perror("socket");
        return 1;
    }

    memset(&server->server_addr, 0, sizeof(server->server_addr)); // initialisation
    server->server_addr.sin_family = AF_INET;
    server->server_addr.sin_addr.s_addr = INADDR_ANY;
    server->server_addr.sin_port = htons(port);

    if (bind(server->server_socket_fd, (struct sockaddr*)&server->server_addr,
             sizeof(server->server_addr)) < 0) {
        perror("bind");
        close(server->server_socket_fd);
        return 1;
    }

    if (listen(server->server_socket_fd, 10) < 0) {
        perror("listen");
        close(server->server_socket_fd);
        return 1;
    }

    std::cout << "Serveur prêt sur le port " << port << "\n";
    return 0;
}