#ifndef SERVER_H
#define SERVER_H

# include "struct.h"

int start_server(int port);
void game_tick_player(struct s_player* player);

#endif
