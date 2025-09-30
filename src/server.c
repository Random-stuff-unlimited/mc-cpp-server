#include "server.h"
#include "struct.h"
#include "queue.h"
#include "protocol.h"
#include "command_manager.h"
#include "network_worker.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/select.h>
#include <strings.h>
#include <time.h>

void    game_tick_player(t_player *player)
{
	(void)player;
	// player->x++;
	// player->y++;
}

void	*network_thread(void* arg)
{
    t_server	*server = (t_server *)arg;
    fd_set read_fds;
    int max_fd;
    while (server->stop_server == 0)
	{
        FD_ZERO(&read_fds);
        max_fd = server->server_socket_fd;
        FD_SET(server->server_socket_fd, &read_fds);

        pthread_mutex_lock(&server->player_lock);
        for (unsigned int i = 0; i < server->player_max; i++) {
            t_player *p = &server->players_lst[i];
            if (p && p->connected)
			{
                FD_SET(p->socket_fd, &read_fds);
                if (p->socket_fd > max_fd)
					max_fd = p->socket_fd;
            }
        }
        pthread_mutex_unlock(&server->player_lock);
        if (select(max_fd+1, &read_fds, NULL, NULL, NULL) > 0) {
			
            if (FD_ISSET(server->server_socket_fd, &read_fds)) {
                struct sockaddr_in client_addr;
                socklen_t addr_len = sizeof(client_addr);
                int client_fd = accept(server->server_socket_fd, (struct sockaddr*)&client_addr, &addr_len);
				if (client_fd >= 0)
				{
					int next_state = handle_handshake(client_fd);
					if (next_state == 1)
					{
						handle_status_request(client_fd);
						close(client_fd);
					}
					else if (next_state == 2)
					{
						if (handle_login_start(client_fd, server) != 0)
							close(client_fd);
					}
					else
						close(client_fd);
				}
				else
					perror("accept");
            }

            pthread_mutex_lock(&server->player_lock);
            for (unsigned int i = 0; i < server->player_max; i++)
			{
                t_player *p = &server->players_lst[i];
                if (p && p->connected && FD_ISSET(p->socket_fd, &read_fds)) {
                    unsigned char buffer[BUFFER_SIZE];
                    int n = read(p->socket_fd, buffer, BUFFER_SIZE);
                    if (n > 0) {
                        t_packet pkt;
                        pkt.player = p;
                        pkt.len = n;
                        memcpy(pkt.data, buffer, n);
                        enqueue(&server->packet_queue, pkt);
                    } else {
                        close(p->socket_fd);
                        p->connected = 0;
                    }
                }
            }
            pthread_mutex_unlock(&server->player_lock);
        }
    }
    return (NULL);
}

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
				game_tick_player(p);
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
	return (NULL);
}

int	start_server(int port)
{
	t_server	server;

	//server start
	if (init_server(&server, port) == 1) // pas encore secure
	{
		clear_memory(&server);
		return (1);
	}

	printf("Server Listening on port: %d\n", port);

	// while attente user entry
	fd_set read_fds;
    while (1)
    {
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        int max_fd = STDIN_FILENO;

        printf("> ");
        fflush(stdout);

        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) > 0)
        {
            if (FD_ISSET(STDIN_FILENO, &read_fds))
            {
                char cmd[1024];
                if (fgets(cmd, sizeof(cmd), stdin))
                {
                    if (cmd_manager(cmd) == 1)
                        break;
                }
            }
        }
    }

	// stop server

	// stop thread
	server.stop_server = 1;
	for (int i = 0; i < THREAD_POOL_SIZE; i++)
		pthread_join(server.threads_worker[i], NULL);
	pthread_join(server.thread_tick, NULL);
	pthread_join(server.thread_network_manager, NULL);

	// kill mutex
	for (unsigned int i = 0; i < server.player_max; i++)
		pthread_mutex_destroy(&server.players_lst[i].lock);
	pthread_mutex_destroy(&server.player_lock);

	//free memory
	free(server.players_lst);
	close(server.server_socket_fd);
	return (0);
}
