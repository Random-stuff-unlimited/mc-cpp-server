#ifndef STRUCT_H
# define STRUCT_H

# include <pthread.h>
# include <netinet/in.h>
# include <sys/time.h>

# define MAX_PLAYERS 1024
# define BUFFER_SIZE 4096
# define THREAD_POOL_SIZE 8
# define TICK_MS 50

typedef struct s_server t_server; 

typedef struct s_player
{
	int				socket_fd;
	char			username[32];
	int				x, y, z;
	int				health;
	int				connected;
	pthread_mutex_t	lock;
	char			lock_init;
}	t_player;

typedef struct s_packet
{
	unsigned char	data[BUFFER_SIZE];
	int				len;
	t_player		*player;
}	t_packet;

typedef struct s_node
{
	t_packet		packet;
	struct s_node	*next;
}	t_node;

typedef struct s_packet_queue
{
	t_node			*head;
	t_node			*tail;
	pthread_mutex_t	lock;
	pthread_cond_t	cond;
	char			init;
	char			shutdown;
}	t_packet_queue;

typedef struct s_thread
{
	t_server		*server;
	unsigned int	id;
}	t_thread;

typedef struct s_server
{
	unsigned int		player_max;
	t_player			*players_lst;
	pthread_mutex_t		player_lock;
	pthread_mutex_t		stop_thread_lock;
	char				stop_lock_init;
	int					server_socket_fd;
	struct sockaddr_in	server_addr;
	t_packet_queue		packet_queue;
	char				queue_init;
	pthread_t			threads_worker[THREAD_POOL_SIZE];
	char				threads_worker_init[THREAD_POOL_SIZE];
	char				threads_worker_done[THREAD_POOL_SIZE];
	t_thread			threads_args[THREAD_POOL_SIZE];
	pthread_t			thread_network_manager;
	char				thread_network_manager_init;
	char				thread_network_manager_done;
	pthread_t			thread_tick;
	char				thread_tick_init;
	char				thread_tick_done;
	_Atomic char		stop_server;
	char				player_lock_init;
}	t_server;


#endif
