#include "server.h"
#include "struct.h"
#include "libft.h"

int	init_player(t_server *server)
{
	server->players_lst = malloc(sizeof(t_player) * server->player_max);
	if (!server->players_lst)
		return (1);
	for (unsigned int i = 0; i < server->player_max; i++)
	{
		server->players_lst[i].connected = 0;
		server->players_lst[i].socket_fd = -1;
		server->players_lst[i].lock_init = 0;
	}
	return (0);
}

int	init_server(t_server *server, int port)
{
	ft_bzero(server, sizeof(t_server));
	if ((init_network(server, port) == 1) || (init_player(server) == 1))
		return (1);
	return ((init_mutex(server) == 1) || (init_thread_pool(server) == 1));
}
