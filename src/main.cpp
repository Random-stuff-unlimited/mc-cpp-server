#include "server.hpp"

int main()
{
	Server server;

	return (server.start_server(25565));
}