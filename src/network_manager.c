#include "network.h"
#include "struct.h"
#include "libft.h"
#include "queue.h"
#include "protocol.h"
#include <pthread.h>
#include <stdio.h>

void	*network_thread(void* arg)
{
    t_server	*server = (t_server *)arg;
    fd_set read_fds;
    int max_fd;
    struct timeval timeout;
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

        timeout.tv_sec = 0;
        timeout.tv_usec = 1000; 
        int select_result = select(max_fd+1, &read_fds, NULL, NULL, &timeout);
        if (select_result > 0) {
			
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
				else if (server->stop_server == 0)
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
                        ft_memcpy(pkt.data, buffer, n);
                        enqueue(&server->packet_queue, pkt);
                    } else {
                        close(p->socket_fd);
                        p->connected = 0;
                    }
                }
            }
            pthread_mutex_unlock(&server->player_lock);
        }
        else if (select_result < 0 && server->stop_server == 0)
        {
            perror("select error");
            break;
        }
    }
	pthread_mutex_lock(&server->stop_thread_lock);
	server->thread_network_manager_done = 1;
	pthread_mutex_unlock(&server->stop_thread_lock);
    return (NULL);
}
