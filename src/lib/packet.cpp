#include "lib/json.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "network/networking.hpp"
#include "lib/compression.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <vector>

using json = nlohmann::json;

Packet::~Packet() {}

Packet::Packet(const Packet& other)
	: _size(other._size), _id(other._id), _data(other._data), _player(other._player), _socketFd(other._socketFd), _returnPacket(other._returnPacket) {
}

Packet& Packet::operator=(const Packet& other) {
	if (this != &other) {
		_size		  = other._size;
		_id			  = other._id;
		_data		  = other._data;
		_player		  = other._player;
		_socketFd	  = other._socketFd;
		_returnPacket = other._returnPacket;
	}
	return (*this);
}

Packet::Packet(std::shared_ptr<Player> player, int32_t id, const std::vector<uint8_t>& payload, int32_t size)
	: _size(size), _id(id), _data(payload), _player(std::move(player)), _socketFd(-1), _returnPacket(PACKET_OK) {
	if (_player == nullptr) throw std::runtime_error("Packet init with null player");
	_socketFd = _player->getSocketFd();
}

Packet::Packet(std::shared_ptr<Player> player) : _size(0), _id(0), _data(), _player(std::move(player)), _socketFd(-1), _returnPacket(PACKET_OK) {
	if (_player == nullptr) throw std::runtime_error("Packet init with null player");
	_socketFd = _player->getSocketFd();
}

int Packet::getVarintSize(int32_t value) {
	if (value < 0) {
		std::cerr << "[Packet] ERROR: getVarintSize called with negative value: " << value << std::endl;
		throw std::runtime_error("getVarintSize called with negative value");
	}
	int size		   = 0;
	int original_value = value;
	do {
		value >>= 7;
		size++;
	} while (value != 0);
	// g_logger->logNetwork(INFO, "getVarintSize(" + std::to_string(original_value) + ") = " +
	// std::to_string(size), "Packet");
	return size;
}

void Packet::setReturnPacket(int value) { this->_returnPacket = value; }
int	 Packet::getReturnPacket() { return (this->_returnPacket); }

int Packet::varintLen(int value) {
	int len = 0;
	do {
		len++;
		value >>= 7;
	} while (value != 0);
	return (len);
}

void Packet::sendPacket(int id, Buffer& data, Server& server) { send(_player, id, data, server); }

void Packet::send(const std::shared_ptr<Player>& player, int id, Buffer& data, Server& server) {
	Packet* out = new Packet(player);
	out->_data	= Buffer(buildFrame(id, data.getData(), player->getCompressionThreshold()));
	out->_id	= id;
	out->_size	= out->_data.getData().size();
	out->_returnPacket = PACKET_SEND;
	server.getNetworkManager().enqueueOutgoingPacket(out);
}

void Packet::sendFrame(const std::shared_ptr<Player>& player, std::vector<uint8_t> frame, Server& server) {
	Packet* out = new Packet(player);
	out->_size	= frame.size();
	out->_data	= Buffer(std::move(frame));
	out->_returnPacket = PACKET_SEND;
	server.getNetworkManager().enqueueOutgoingPacket(out);
}

std::vector<uint8_t> Packet::buildFrame(int id, const std::vector<uint8_t>& data, int compressionThreshold, int compressionLevel) {
	Buffer body;
	body.writeVarInt(id);
	body.writeBytes(data);

	Buffer frame;
	if (compressionThreshold < 0) {
		frame.writeVarInt(static_cast<int32_t>(body.getData().size()));
		frame.writeBytes(body.getData());
		return std::move(frame.getData());
	}

	Buffer inner;
	if (static_cast<int>(body.getData().size()) < compressionThreshold) {
		inner.writeVarInt(0); // Not compressed
		inner.writeBytes(body.getData());
	} else {
		inner.writeVarInt(static_cast<int32_t>(body.getData().size()));
		inner.writeBytes(compression::zlibCompress(body.getData().data(), body.getData().size(), compressionLevel));
	}
	frame.writeVarInt(static_cast<int32_t>(inner.getData().size()));
	frame.writeBytes(inner.getData());
	return std::move(frame.getData());
}

Player*	 Packet::getPlayer() const { return (_player.get()); }
uint32_t Packet::getSize() { return (_size); }
uint32_t Packet::getId() { return (_id); }
Buffer&	 Packet::getData() { return (_data); }
int		 Packet::getSocket() const { return (_socketFd); };
void	 Packet::setPacketSize(int32_t value) { _size = value; }
void	 Packet::setPacketId(uint32_t value) { _id = value; }
