#include "server.h"

int init_network(t_server *server, int port)
{
	server->server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server->server_socket_fd < 0)
	{
		perror("socket");
		free(server->players_lst);
		return (1);
	}
	server->server_addr.sin_family = AF_INET;
	server->server_addr.sin_addr.s_addr = INADDR_ANY;
	server->server_addr.sin_port = htons(port);
	if (bind(server->server_socket_fd, (struct sockaddr*)&server->server_addr, sizeof(server->server_addr)) < 0)
	{
		perror("bind");
		free(server->players_lst);
		close(server->server_socket_fd);
		return (1);
	}
	if (listen(server->server_socket_fd, 10) < 0)
	{
		perror("listen");
		free(server->players_lst);
		close(server->server_socket_fd);
		return (1);
	}
	return (0);
}
