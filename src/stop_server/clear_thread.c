#include "server.h"
#include "struct.h"
#include <unistd.h>

void wait_and_clear_threads(t_server *server)
{
	char	finish;

	server->stop_server = 1;
	for (int i = 0; i < THREAD_POOL_SIZE; i++)
	{
		if (server->threads_worker_init[i])
		{
			do
			{
				printf("Waiting for worker thread %d to finish...\n", i);
				pthread_mutex_lock(&server->stop_thread_lock);
				finish = server->threads_worker_done[i];
				pthread_mutex_unlock(&server->stop_thread_lock);
				usleep(1000);
				printf("Worker thread %d done status: %d\n", i, finish);
			} while (finish == 0);
			server->threads_worker_init[i] = 0;
		}
	}
	if (server->thread_tick_init)
	{
		do
		{
			printf("Waiting for worker thread thread_tick_done to finish...\n");
			pthread_mutex_lock(&server->stop_thread_lock);
			finish = server->thread_tick_done;
			pthread_mutex_unlock(&server->stop_thread_lock);
			usleep(1000);
			printf("Worker thread thread_tick_done done status: %d\n", finish);
		} while (finish == 0);
		server->thread_tick_init = 0;
	}
	if (server->thread_network_manager_init)
	{
		do
		{
			printf("Waiting for worker thread thread_network_manager_done to finish...\n");
			pthread_mutex_lock(&server->stop_thread_lock);
			finish = server->thread_network_manager_done;
			pthread_mutex_unlock(&server->stop_thread_lock);
			usleep(1000);
			printf("Worker thread thread_network_manager_done done status: %d\n", finish);
		} while (finish == 0);
		server->thread_network_manager_init = 0;
	}
}