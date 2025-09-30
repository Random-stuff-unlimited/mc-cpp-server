#include "tick_manager.h"
#include <stdio.h>
#include <unistd.h>

void *tick_thread(void *arg)
{
	t_server	*server = (t_server *)arg;
	struct timespec start, end;
	while (server->stop_server == 0)
	{
		clock_gettime(CLOCK_MONOTONIC, &start);
		pthread_mutex_lock(&server->player_lock);
		for (unsigned int i = 0; i < server->player_max; i++)
		{
			t_player *p = &server->players_lst[i];
			if (p && p->connected)
			{
				pthread_mutex_lock(&p->lock);
				pthread_mutex_unlock(&p->lock);
			}
		}
		pthread_mutex_unlock(&server->player_lock);
		clock_gettime(CLOCK_MONOTONIC, &end);
		long elapsed_ms =
			(end.tv_sec - start.tv_sec) * 1000 +
			(end.tv_nsec - start.tv_nsec) / 1000000;
		long sleep_ms = TICK_MS - elapsed_ms;
		if (sleep_ms > 0)
			usleep(sleep_ms * 1000);
		else
			printf("⚠ Tick en retard (%ld ms)\n", elapsed_ms);
	}
	pthread_mutex_lock(&server->stop_thread_lock);
	server->thread_tick_done = 1;
	pthread_mutex_unlock(&server->stop_thread_lock);
	return (NULL);
}
