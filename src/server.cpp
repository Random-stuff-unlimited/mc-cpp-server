#include "server.hpp"
#include <sys/socket.h>
#include <iostream>

Server::Server() : _port(0), _player_lst(nullptr), _socket(-1), _stopThread(false) {}
Server::Server(const Server &src) {}
Server &Server::operator=(const Server &src) {}
Server::~Server() {}

int		Server::getServerPort() {return (this->_port);}
void	Server::setServerPort(int port) {this->_port = port;}
int		Server::getServerAddr() {return (this->_addr);}
void	Server::setServerAddr(int addr) {this->_addr = addr;}

int	Server::init_thread()
{
	try {
		_networkManager = std::thread(&Server::networkManagerLoop, this);
		// for (int i = 0; i < 10; i++)
		// 	_networkWorker[i] = std::thread(&Server::networkWorkerLoop, this, i);
		_tickManager = std::thread(&Server::tickLoop, this);
	} catch (const std::system_error& e) {
		std::cerr << "Error in thread creation : " << e.what() << "\n";
		
		return (1);
	}
}

int Server::start_server(int port) {
	if (init_network(port) != 0 || )
		return (1);
}

