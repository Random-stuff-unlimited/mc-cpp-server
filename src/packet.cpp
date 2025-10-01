#include "packet.hpp"
#include "buffer.hpp"
#include <unistd.h>
#include <iostream>
#include <vector>
#include "json.hpp"

using json = nlohmann::json;

Packet::Packet(Player *player) : _player(player), _socketFd(-1) {
	_size = readVarint(_socketFd);
	_id = readVarint(_socketFd);
	if (player != NULL)
		_socketFd =  _player->getSocketFd();
	if (_size > 0) {
		std::vector<uint8_t> tmp(_size);
		ssize_t bytesRead = ::read(_socketFd, tmp.data(), _size);
		if (bytesRead != _size) {
			throw std::runtime_error("error on packet reading");
		}
		_data = Buffer(tmp);
	}
}

Packet::Packet(int socketFd, Server &server) : _player(NULL), _socketFd(socketFd) {
	_size = readVarint(_socketFd);
	_id = readVarint(_socketFd);
	try {
		_player = new Player();
		_player->setSocketFd(_socketFd);
		_player->getPlayerState();
		try
		{
			server.addPlayer(_player);
		}
		catch(const std::exception& e)
		{
			delete _player;
			std::cerr << e.what() << '\n';
		}
	} catch(const std::exception& e) {
		throw std::runtime_error("error on packet player init");
	}
	
	if (_size > 0) {
		std::vector<uint8_t> tmp(_size);
		ssize_t bytesRead = ::read(_socketFd, tmp.data(), _size);
		if (bytesRead != _size) {
			throw std::runtime_error("error on packet reading");
		}
		_data = Buffer(tmp);
	}
}

int Packet::readVarint(int sock) {
	int value = 0, position = 0;
	uint8_t byte;
	while (true) {
		if (::read(sock, &byte, 1) <= 0) return -1;
		value |= (byte & 0x7F) << position;
		if (!(byte & 0x80)) break;
		position += 7;
		if (position >= 32) return -1;
	}
	return value;
}

void Packet::writeVarint(int sock, int value) {
	std::vector<uint8_t> tmp;
	Buffer buf(tmp);
	buf.writeVarInt(value);
	::write(sock, buf.getData().data(), buf.getData().size());
}

int Packet::varintLen(int value) {
	int len = 0;
	do {
		len++;
		value >>= 7;
	} while (value != 0);
	return (len);
}

Player *Packet::getPlayer() const {return (_player);}
uint32_t Packet::getSize() {return (_size);}
uint32_t Packet::getId() {return (_id);}
Buffer	&Packet::getData() {return (_data);}
int	Packet::getSocket() const {return (_socketFd);};
