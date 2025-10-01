#include "network.hpp"
#include "server.hpp"
#include <unistd.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include "packet.hpp"

Network::Network(int port, Server &server)
    : server(server)
    , _port(port)
    , _socket(-1)
    , _addr{}
    , _networkManagerRun(false)
    , _networkManagerInit(0)
    , _networkWorkerInit{}
    , _networkWorkerRun{}
    , _stop(false)
{
    _socket = socket(AF_INET, SOCK_STREAM, 0);
    if (_socket < 0) {
        perror("socket");
        throw std::runtime_error("socket error");
    }

    memset(&_addr, 0, sizeof(_addr));
    _addr.sin_family = AF_INET;
    _addr.sin_addr.s_addr = INADDR_ANY;
    _addr.sin_port = htons(_port);

    if (bind(_socket, (struct sockaddr*)&_addr, sizeof(_addr)) < 0) {
        perror("bind");
        close(_socket);
        throw std::runtime_error("bind error");
    }

    if (listen(_socket, 10) < 0) {
        perror("listen");
        close(_socket);
        throw std::runtime_error("listen error");
    }

    std::cout << "Network ready on port " << _port << "\n";

    try {
        startThreads(server);
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << "\n";
    }
}

Network::~Network() {
    stopThreads();
    if (_socket != -1) {
        close(_socket);
    }
}

void Network::startThreads(Server &server)
{
	_networkManagerInit = 0;
	_networkManagerRun = 0;
	for (int i = 0; i < 10; ++i)
	{
		_networkWorkerInit[i] = 0;
		_networkWorkerRun[i] = 0;
	}
	try {
		_networkManager = std::thread(&Network::networkManagerLoop, this, std::ref(server));
		_networkManager.detach();
		_networkManagerInit = 1;
		for (int i = 0; i < 10; ++i)
		{
			_networkWorkerInit[i] = 0;
			_networkWorkerRun[i] = 0;
			// _networkWorker[i] = std::thread(&Network::networkWorkerLoop, this, i);
			// _networkWorker[i].detach();
		}
	} catch (const std::exception& e) {
		stopThreads();
		throw std::runtime_error(std::string("Thread start error: ") + e.what());
	}
}

void Network::stopThreads()
{
	_stop = true;

	if (_networkManagerInit == 1)
		while (_networkManagerRun)
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
	for (size_t i = 0; i < 10; i++)
		if (_networkWorkerInit[i] == 1)
			while (_networkWorkerRun[i] == true)
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

void Network::networkManagerLoop(Server &server)
{
	fd_set read_fds;
    int max_fd;
	struct timeval net_timeout;
	Packet	packet;

	_networkManagerRun = true;
	while (!_stop)
	{
		FD_ZERO(&read_fds);
		max_fd = _socket;
		FD_SET(_socket, &read_fds);

        // pthread_mutex_lock(&server->player_lock);
        // for (unsigned int i = 0; i < server->player_max; i++) {
        //     t_player *p = &server->players_lst[i];
        //     if (p && p->connected)
		// 	{
        //         FD_SET(p->socket_fd, &read_fds);
        //         if (p->socket_fd > max_fd)
		// 			max_fd = p->socket_fd;
        //     }
        // }
        // pthread_mutex_unlock(&server->player_lock);

		net_timeout.tv_sec = 0;
		net_timeout.tv_usec = 1000;

		int select_result = select(max_fd + 1, &read_fds, nullptr, nullptr, &net_timeout);
		if (select_result > 0) {
			if (FD_ISSET(_socket, &read_fds)) {
				sockaddr_in client_addr{};
				socklen_t addr_len = sizeof(client_addr);
				int client_fd = accept(_socket, (sockaddr*)&client_addr, &addr_len);
				if (client_fd >= 0) {
					int next_state = packet.handleHandshake(client_fd);
					if (next_state == 1)
					{
						packet.handleStatusRequest(client_fd, server);
						close(client_fd);
					}
					else if (next_state == 2)
					{
						// if (handle_login_start(client_fd, server) != 0)
						std::cout << "try to connect" << std::endl;
						close(client_fd);
					}
					else
						close(client_fd);
				} else {
					perror("accept");
				}
			}

			// données clients existants
            // pthread_mutex_lock(&server->player_lock);
            // for (unsigned int i = 0; i < server->player_max; i++)
			// {
            //     t_player *p = &server->players_lst[i];
            //     if (p && p->connected && FD_ISSET(p->socket_fd, &read_fds)) {
            //         unsigned char buffer[BUFFER_SIZE];
            //         int n = read(p->socket_fd, buffer, BUFFER_SIZE);
            //         if (n > 0) {
            //             t_packet pkt;
            //             pkt.player = p;
            //             pkt.len = n;
            //             ft_memcpy(pkt.data, buffer, n);
            //             enqueue(&server->packet_queue, pkt);
            //         } else {
            //             close(p->socket_fd);
            //             p->connected = 0;
            //         }
            //     }
            // }
            // pthread_mutex_unlock(&server->player_lock);
		} else if (select_result < 0) {
			perror("select error");
			break;
		}
	}
	_networkManagerRun = false;
}



