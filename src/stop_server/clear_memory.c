#include "server.h"
#include "struct.h"

void clear_mutex(t_server *server)
{
	for (unsigned int i = 0; i < server->player_max; i++)
	{
		if (server->players_lst[i].lock_init)
			pthread_mutex_destroy(&server->players_lst[i].lock);
	}
	if (server->player_lock_init)
		pthread_mutex_destroy(&server->player_lock);
	if (server->packet_queue.init != -1)
		pthread_mutex_destroy(&server->packet_queue.lock);
	if (server->packet_queue.init != -1 && server->packet_queue.init != -2)
		pthread_cond_destroy(&server->packet_queue.cond);
	if (server->stop_lock_init)
		pthread_mutex_destroy(&server->stop_thread_lock);
}

void	clear_memory(t_server *server)
{
	clear_mutex(server);
	wait_and_clear_threads(server);
}