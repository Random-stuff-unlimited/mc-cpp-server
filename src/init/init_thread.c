#include "server.h"

int init_thread_pool(t_server *server) // pas encore secure
{
	init_queue(&server->packet_queue);

	for (int i = 0; i < THREAD_POOL_SIZE; i++)
	{
		pthread_create(&server->threads_worker[i], NULL, network_worker, (void *)server); // add une struct pour la packet queue
		pthread_detach(server->threads_worker[i]);
	}

	pthread_create(&server->thread_tick, NULL, tick_thread, (void *)server);
	pthread_detach(server->thread_tick);

	pthread_create(&server->thread_network_manager, NULL, network_thread, (void *)server); // add une struct pour la packet queue
	pthread_detach(server->thread_network_manager);
	return (0);
}
