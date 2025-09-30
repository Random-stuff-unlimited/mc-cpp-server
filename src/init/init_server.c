#include "server.h"



int	init_server(t_server *server, int port) // pas encore secure
{
	bzero(server, sizeof(t_server));
	if (init_network(server, port) == 1)
		return (1);
	server->players_lst = malloc(sizeof(t_player) * server->player_max);
	if (!server.players_lst)
		return (1);
	for (unsigned int i = 0; i < server->player_max; i++)
	{
		server->players_lst[i].connected = 0;
		server->players_lst[i].socket_fd = -1;
		if (pthread_mutex_init(&server->players_lst[i].lock, NULL) < 0)
		{
			for (unsigned int j = 0; j < i; j++)
				pthread_mutex_destroy(&server.players_lst[j].lock);
			break
		}
	}
	if ()
	pthread_mutex_init(&server->player_lock, NULL);
	return (init_thread_pool(server));
	return (0);
}
