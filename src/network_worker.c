#include "network.h"
#include "struct.h"
#include "queue.h"
#include <pthread.h>
#include <stdio.h>

void    *network_worker(void *arg)
{
	t_thread	*thread_data = (t_thread *)arg;
	t_server	*server = thread_data->server;
	while (server->stop_server == 0)
	{
		t_packet p = dequeue(&server->packet_queue);
		if (!p.player || !p.player->connected) continue;

		pthread_mutex_lock(&p.player->lock);
		// add packet 
		printf("Packet reçu de %s (len=%d)\n", p.player->username, p.len);
		pthread_mutex_unlock(&p.player->lock);
	}
	pthread_mutex_lock(&server->stop_thread_lock);
	server->threads_worker_done[thread_data->id] = 1;
	pthread_mutex_unlock(&server->stop_thread_lock);
	return NULL;
}