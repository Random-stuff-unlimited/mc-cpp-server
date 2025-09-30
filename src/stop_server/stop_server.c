#include "server.h"
#include <stdlib.h>
#include <unistd.h>

void	stop_server(t_server *server)
{
	server->stop_server = 1;
	clear_memory(server);
	free(server->players_lst);
	close(server->server_socket_fd);
}
