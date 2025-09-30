#include "server.h"
#include "struct.h"

void clear_mutex(t_server *server, unsigned int max)
{
	for (unsigned int i = 0; i < max; i++)
		pthread_mutex_destroy(server.players_lst[i].lock);
}
 