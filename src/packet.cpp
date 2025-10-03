#include "packet.hpp"

#include "buffer.hpp"
#include "enums.hpp"
#include "json.hpp"
#include "player.hpp"
#include "server.hpp"

#include <cstdint>
#include <errno.h>
#include <exception>
#include <iostream>
#include <poll.h>
#include <stdexcept>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

using json = nlohmann::json;

Packet::~Packet() {}

Packet::Packet(Player* player) : _player(player), _socketFd(-1), _returnPacket(0) {
	if (_player == nullptr)
		throw std::runtime_error("Packet init with null player");
	_socketFd = _player->getSocketFd();
	std::cout << "[Packet] Constructor: Socket FD = " << _socketFd << std::endl;

	_size = readVarint(_socketFd);
	if (_size == -1)
		throw std::runtime_error("Failed to read packet size");
	std::cout << "[Packet] Read size: " << _size << std::endl;

	_id = readVarint(_socketFd);
	if (_id == -1)
		throw std::runtime_error("Failed to read packet id");
	std::cout << "[Packet] Read ID: " << _id << std::endl;

	int remaining = _size - getVarintSize(_id);
	std::cout << "[Packet] Calculated remaining: " << remaining << " (size=" << _size
	          << " - varintSize(" << _id << ")=" << getVarintSize(_id) << ")" << std::endl;

	if (remaining < 0)
		throw std::runtime_error("Invalid packet size");
	if (remaining > 0) {
		std::vector<uint8_t> tmp(remaining);
		ssize_t totalRead = 0;

		while (totalRead < remaining) {
			ssize_t bytesRead = ::read(_socketFd, tmp.data() + totalRead, remaining - totalRead);
			if (bytesRead <= 0) {
				throw std::runtime_error("error on packet reading (socket closed or error)");
			}
			totalRead += bytesRead;
		}

		std::cout << "Packet total size: " << _size << std::endl;
		std::cout << "Packet id: " << _id << std::endl;
		std::cout << "Data size: " << remaining << std::endl;
		std::cout << "Read size: " << totalRead << std::endl;

		_data = Buffer(tmp);
	}
}

Packet::Packet(int socketFd, Server& server)
    : _player(nullptr), _socketFd(socketFd), _returnPacket(0) {
	std::cout << "[Packet] Constructor (socket): Socket FD = " << _socketFd << std::endl;

	_size = readVarint(_socketFd);
	if (_size == -1)
		throw std::runtime_error("Failed to read packet size");
	std::cout << "[Packet] Read size: " << _size << std::endl;

	_id = readVarint(_socketFd);
	if (_id == -1)
		throw std::runtime_error("Failed to read packet id");
	std::cout << "[Packet] Read ID: " << _id << std::endl;

	int remaining = _size - (getVarintSize(_id));
	std::cout << "[Packet] Calculated remaining: " << remaining << " (size=" << _size
	          << " - varintSize(" << _id << ")=" << getVarintSize(_id) << ")" << std::endl;

	if (remaining < 0)
		throw std::runtime_error("Invalid packet size");

	try {
		_player = server.addPlayer("None", PlayerState::Handshake, socketFd);
	} catch (const std::exception& e) {
		_player = nullptr;
		throw std::runtime_error("error on packet player init");
	}

	if (remaining > 0) {
		std::vector<uint8_t> tmp(remaining);
		ssize_t totalRead = 0;

		while (totalRead < remaining) {
			ssize_t bytesRead = ::read(_socketFd, tmp.data() + totalRead, remaining - totalRead);
			if (bytesRead <= 0) {
				throw std::runtime_error("error on packet reading (socket closed or error)");
			}
			totalRead += bytesRead;
		}

		std::cout << "Packet total size: " << _size << std::endl;
		std::cout << "Packet id: " << _id << std::endl;
		std::cout << "Data size: " << remaining << std::endl;
		std::cout << "Read size: " << totalRead << std::endl;

		_data = Buffer(tmp);
	}
}

int Packet::getVarintSize(int32_t value) {
	if (value < 0) {
		std::cerr << "[Packet] ERROR: getVarintSize called with negative value: " << value
		          << std::endl;
		throw std::runtime_error("getVarintSize called with negative value");
	}
	int size           = 0;
	int original_value = value;
	do {
		value >>= 7;
		size++;
	} while (value != 0);
	std::cout << "[Packet] getVarintSize(" << original_value << ") = " << size << std::endl;
	return size;
}

int Packet::readVarint(int sock) {
	// Validate socket first
	if (!isSocketValid(sock)) {
		return -1;
	}

	int value = 0, position = 0;
	uint8_t byte;
	int bytesRead = 0;

	while (true) {
		ssize_t result = ::read(sock, &byte, 1);
		if (result <= 0) {
			std::cerr << "readVarint: Failed to read byte " << bytesRead << " from socket " << sock
			          << " (errno: " << errno << ")" << std::endl;
			return -1;
		}

		bytesRead++;
		value |= (byte & 0x7F) << position;

		if (!(byte & 0x80))
			break; // Last byte of varint

		position += 7;
		if (position >= 32) {
			std::cerr << "readVarint: Varint too long (> 32 bits) after " << bytesRead << " bytes"
			          << std::endl;
			return -1;
		}

		// Safety check to prevent infinite loops
		if (bytesRead > 5) {
			std::cerr << "readVarint: Too many bytes read (" << bytesRead << "), corrupted varint"
			          << std::endl;
			return -1;
		}
	}

	std::cout << "readVarint: Successfully read " << value << " (" << bytesRead << " bytes)"
	          << std::endl;
	return value;
}

void Packet::writeVarint(int sock, int value) {
	std::vector<uint8_t> tmp;
	Buffer buf(tmp);
	buf.writeVarInt(value);
	::write(sock, buf.getData().data(), buf.getData().size());
}

bool Packet::isSocketValid(int sock) {
	if (sock < 0) {
		std::cerr << "Socket validation: Invalid descriptor " << sock << std::endl;
		return false;
	}

	// Check if socket is still connected using poll or similar
	struct pollfd pfd;
	pfd.fd     = sock;
	pfd.events = POLLIN;

	int result = poll(&pfd, 1, 0); // Non-blocking check
	if (result < 0) {
		std::cerr << "Socket validation: poll() failed with errno " << errno << std::endl;
		return false;
	}

	// If POLLHUP or POLLERR is set, socket is disconnected or has error
	if (pfd.revents & (POLLHUP | POLLERR)) {
		std::cerr << "Socket validation: Socket " << sock << " is disconnected or has error"
		          << std::endl;
		return false;
	}

	return true;
}

void Packet::setReturnPacket(int value) {
	this->_returnPacket = value;
}
int Packet::getReturnPacket() {
	return (this->_returnPacket);
}

int Packet::varintLen(int value) {
	int len = 0;
	do {
		len++;
		value >>= 7;
	} while (value != 0);
	return (len);
}

Player* Packet::getPlayer() const {
	return (_player);
}
uint32_t Packet::getSize() {
	return (_size);
}
uint32_t Packet::getId() {
	return (_id);
}
Buffer& Packet::getData() {
	return (_data);
}
int Packet::getSocket() const {
	return (_socketFd);
};
void Packet::setPacketSize(int32_t value) {
	_size = value;
}
void Packet::setPacketId(int32_t value) {
	_id = value;
}
