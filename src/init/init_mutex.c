#include "server.h"
#include "queue.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>

int init_mutex(t_server *server)
{
	int ret;

	for (unsigned int i = 0; i < server->player_max; i++)
	{
		ret = pthread_mutex_init(&server->players_lst[i].lock, NULL);
		if (ret != 0)
		{
			fprintf(stderr, "Error pthread_mutex_init players_lst %s: %d\n", strerror(ret), i);
			return (1);
		}
		server->players_lst[i].lock_init = 1;
	}
	ret = pthread_mutex_init(&server->player_lock, NULL);
	if (ret != 0)
	{
		fprintf(stderr, "Error pthread_mutex_init player_lock: %s\n", strerror(ret));
		return (1);
	}
	server->player_lock_init = 1;
	ret = init_queue(&server->packet_queue);
	if (ret != 0)
	{
		fprintf(stderr, "Error pthread_mutex_init packet_queue: %s\n", strerror(ret));
		return (1);
	}
	server->queue_init = 1;
	ret = pthread_mutex_init(&server->stop_thread_lock, NULL);
	if (ret != 0)
	{
		fprintf(stderr, "Error pthread_mutex_init stop_thread_lock: %s\n", strerror(ret));
		return (1);
	}
	server->stop_lock_init = 1;
	return (0);
}
