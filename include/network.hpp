#ifndef NETWORK_HPP
# define NETWORK_HPP

# include "server.hpp"
# include <thread>
# include <chrono>
# include <atomic>

class Network {
	private:
		Server	&server;
		int _port;
		int _socket;
		sockaddr_in _addr;

		std::thread			_networkManager;
		std::atomic<bool>	_networkManagerRun;
		char				_networkManagerInit;
		std::thread 		_networkWorker[10];
		char				_networkWorkerInit[10];
		std::atomic<bool>	_networkWorkerRun[10];
		std::atomic<bool>	_stop;

	public:
		Network(int port, Server &server);
		~Network();

		void networkManagerLoop(Server &server);
		void networkWorkerLoop(Server &server, int id);

		void startThreads(Server &server);
		void stopThreads();
};

#endif