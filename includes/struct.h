#ifndef STRUCT_H
# define STRUCT_H

# include <pthread.h>
#include <netinet/in.h>

# define MAX_PLAYERS 1024
# define BUFFER_SIZE 4096
# define THREAD_POOL_SIZE 8
# define TICK_MS 50

typedef struct s_player
{
	int socket_fd;
	char username[32];
	int x, y, z;
	int health;
	int connected;
	pthread_mutex_t lock;
}	t_player;

typedef struct s_packet
{
	unsigned char data[BUFFER_SIZE];
	int			len;
	t_player	*player;
}	t_packet;

typedef struct s_node
{
	t_packet packet;
	struct s_node *next;
}	t_node;

typedef struct s_packet_queue
{
	t_node *head;
	t_node *tail;
	pthread_mutex_t lock;
	pthread_cond_t cond;
}	t_packet_queue;

typedef struct s_server
{
	unsigned int		player_max;
	t_player			*players_lst;
	pthread_mutex_t		player_lock;
	int					server_socket_fd;
	struct sockaddr_in	server_addr;
	t_packet_queue		packet_queue;
	pthread_t			threads_worker[THREAD_POOL_SIZE];
	pthread_t			thread_network_manager;
	pthread_t			thread_tick;
	_Atomic char		stop_server;
}	t_server;

#endif
