#ifndef NETWORK_HPP
# define NETWORK_HPP

class Network {
	private:
		int _port;
		int _socket;
		sockaddr_in _addr;

		std::thread _networkManager;
		std::thread _networkWorker[10];
		std::atomic<bool> _stop;

	public:
		Network(int port);
		~Network();

		void networkManagerLoop();
		void networkWorkerLoop(int id);

		void startThreads();
		void stopThreads();
};

#endif