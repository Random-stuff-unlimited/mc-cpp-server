#include "server.h"
#include "queue.h"
#include "network.h"
#include "tick_manager.h"
#include <string.h>
#include <stdio.h>
#include <pthread.h>

int	init_thread_pool(t_server *server)
{
	int			ret;
	for (int i = 0; i < THREAD_POOL_SIZE; i++)
	{
		server->threads_args[i].server = server;
		server->threads_args[i].id = i;
		ret = pthread_create(&server->threads_worker[i], NULL, network_worker, (void *)(&server->threads_args[i]));
		if (ret != 0)
		{
			fprintf(stderr, "Error pthread_create worker %d: %s\n", i, strerror(ret));
			return (1);
		}
		pthread_detach(server->threads_worker[i]);
		server->threads_worker_init[i] = 1;
		server->threads_worker_done[i] = 0;
	}
	ret = pthread_create(&server->thread_tick, NULL, tick_thread, (void *)server);
	if (ret != 0)
	{
		fprintf(stderr, "Error pthread_create tick: %s\n", strerror(ret));
		return (1);
	}
	pthread_detach(server->thread_tick);
	server->thread_tick_init = 1;
	ret = pthread_create(&server->thread_network_manager, NULL, network_thread, (void *)server);
	if (ret != 0)
	{
		fprintf(stderr, "Error pthread_create network manager: %s\n", strerror(ret));
		return (1);
	}
	pthread_detach(server->thread_network_manager);
	server->thread_network_manager_init = 1;
	return (0);
}