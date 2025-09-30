#include "server.h"
#include "

int	init_mutex(t_server *server)
{
	for (unsigned int i = 0; i < server->player_max; i++)
	{
		server->players_lst[i].connected = 0;
		server->players_lst[i].socket_fd = -1;
		pthread_mutex_init(&server->players_lst[i].lock, NULL);
	}
	pthread_mutex_init(&server->player_lock, NULL);
}