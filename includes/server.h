#ifndef SERVER_H
#define SERVER_H

# include "struct.h"

int		start_server(int port);
void	stop_server(t_server *server);
void 	game_tick_player(struct s_player* player);

int	init_server(t_server *server, int port);
int init_network(t_server *server, int port);
int	init_thread_pool(t_server *server);
int	init_mutex(t_server *server);

void	clear_memory(t_server *server);
void	clear_mutex(t_server *server);
void	wait_and_clear_threads(t_server *server);

#endif
