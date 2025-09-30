#include "server.h"
#include "struct.h"
#include "queue.h"
#include "libft.h"
#include "command_manager.h"
#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

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

	// while attente user entry a fix
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
	stop_server(&server);

	return (0);
}
