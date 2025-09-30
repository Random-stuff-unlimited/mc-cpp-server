#include "command_manager.h"
#include <stdio.h>
#include <string.h>

int cmd_manager(char cmd[1024])
{
	cmd[strcspn(cmd, "\n")] = 0;
	if (strcmp(cmd, "stop") == 0)
	{
		printf("Stopping server...\n");
		return (1);
	}
	return (0);
}
