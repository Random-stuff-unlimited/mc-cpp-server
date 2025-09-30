#include "server.hpp"
#include <sys/socket.h>
#include <iostream>

Server::Server() {}
Server::Server(const Server &src) {}
Server &Server::operator=(const Server &src) {}
Server::~Server() {}

int		Server::getServerPort() {return (this->_port);}
void	Server::setServerPort(int port) {this->_port = port;}

int Server::start_server(int port)
{
	
}

