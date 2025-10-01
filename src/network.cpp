#include "network.hpp"
#include <iostream>

Network::Network(int port) : _port(port), _socket(-1), _stop(false) {
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

    startThreads();
}

void Network::startThreads() {
    try {
        _networkManager = std::thread(&Network::networkManagerLoop, this);
        for (int i = 0; i < 10; ++i)
            _networkWorker[i] = std::thread(&Network::networkWorkerLoop, this, i);
    } catch (const std::exception& e) {
        _stop = true;
        if (_networkManager.joinable()) _networkManager.join();
        for (int i = 0; i < 10; ++i)
            if (_networkWorker[i].joinable()) _networkWorker[i].join();

        close(_socket);
        throw std::runtime_error(std::string("Thread start error: ") + e.what());
    }
}

void Network::stopThreads() {
    _stop = true;

    if (_networkManager.joinable()) _networkManager.join();
    for (int i = 0; i < 10; ++i)
        if (_networkWorker[i].joinable())
            _networkWorker[i].join();
}
