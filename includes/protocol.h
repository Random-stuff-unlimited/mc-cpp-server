#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "struct.h"

int     handle_login_start(int client_sock, t_server *server);
void    handle_status_request(int client_sock);
int     handle_handshake(int client_sock);

#endif
