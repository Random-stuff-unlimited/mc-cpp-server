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
    while (1) {
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
    return NULL;
}

void *tick_thread(void *arg)
{
	t_server	*server = (t_server *)arg;
	struct timespec start, end;
	while (1)
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

int init_network(t_server *server, int port)
{
	server->server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server->server_socket_fd < 0)
	{
		perror("socket");
		free(server->players_lst);
		return (1);
	}
	server->server_addr.sin_family = AF_INET;
	server->server_addr.sin_addr.s_addr = INADDR_ANY;
	server->server_addr.sin_port = htons(port);
	if (bind(server->server_socket_fd, (struct sockaddr*)&server->server_addr, sizeof(server->server_addr)) < 0)
	{
		perror("bind");
		free(server->players_lst);
		close(server->server_socket_fd);
		return (1);
	}
	if (listen(server->server_socket_fd, 10) < 0)
	{
		perror("listen");
		free(server->players_lst);
		close(server->server_socket_fd);
		return (1);
	}
	return (0);
}

int	init_server(t_server *server, int port) // pas encore secure
{
	server->player_max = 0;
	server->players_lst = malloc(sizeof(t_player) * server->player_max);
	for (unsigned int i = 0; i < server->player_max; i++)
	{
		server->players_lst[i].connected = 0;
		server->players_lst[i].socket_fd = -1;
		pthread_mutex_init(&server->players_lst[i].lock, NULL);
	}
	pthread_mutex_init(&server->player_lock, NULL);
	init_network(server, port);
	return (init_thread_pool(server));
	return (0);
}

int	start_server(int port)
{
	t_server	server;

	if (init_server(&server, port) == 1) // pas encore secure
	{
		free(server.players_lst);
		return (1);
	}

	//server start
	printf("Server Listening on port: %d\n", port);
	while (1)
	{
		// FD_ZERO(&read_fds);
		// FD_SET(server.server_socket_fd, &read_fds);
		// FD_SET(STDIN_FILENO, &read_fds);

		// printf("> ");
		// fflush(stdout);
		// select(max_fd + 1, &read_fds, NULL, NULL, NULL);

		// if (FD_ISSET(STDIN_FILENO, &read_fds))
		// {
		// 	char cmd[1024];
		// 	if (fgets(cmd, sizeof(cmd), stdin))
		// 		if (cmd_manager(cmd) == 1)
		// 			break;
		// }

	}
	// free(server.players_lst);
	// close(server.server_socket_fd);
	return (0);
}
