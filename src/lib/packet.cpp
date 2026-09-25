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

Packet::Packet(std::shared_ptr<Player> player, int32_t id, std::vector<uint8_t> payload, int32_t size)
	: _size(size), _id(id), _data(std::move(payload)), _player(std::move(player)), _socketFd(-1), _returnPacket(PACKET_OK) {
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
	int size = 0;
	do {
		value >>= 7;
		size++;
	} while (value != 0);
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

namespace {
	void writeVarInt(std::vector<uint8_t>& out, uint32_t value) {
		while (value >= 0x80) {
			out.push_back(static_cast<uint8_t>(value | 0x80));
			value >>= 7;
		}
		out.push_back(static_cast<uint8_t>(value));
	}

	// Id + payload of the compressed packets, reused by each thread
	thread_local std::vector<uint8_t> t_body;
	thread_local std::vector<uint8_t> t_compressed;

	// Appends to the player's output, unless it is disconnecting
	template <typename Append> void appendToOutput(const std::shared_ptr<Player>& player, Append append) {
		PlayerOutput& output = player->output();
		bool		  wasEmpty;
		{
			std::lock_guard<std::mutex> lock(output.mutex);
			if (output.closing) return;
			wasEmpty = output.data.empty();
			append(output.data);
		}
		// Whoever makes it non-empty gets it written
		if (wasEmpty) NetworkManager::markForFlush(player);
	}
} // namespace

void Packet::send(const std::shared_ptr<Player>& player, int id, Buffer& data, Server& server) {
	const std::vector<uint8_t>& payload	  = data.getData();
	int							threshold = player->getCompressionThreshold();
	appendToOutput(player, [&](std::vector<uint8_t>& out) { appendFrame(out, id, payload.data(), payload.size(), threshold); });
	(void)server;
}

void Packet::sendFrame(const std::shared_ptr<Player>& player, const std::vector<uint8_t>& frame, Server& server) {
	appendToOutput(player, [&](std::vector<uint8_t>& out) { out.insert(out.end(), frame.begin(), frame.end()); });
	(void)server;
}

std::vector<uint8_t> Packet::buildFrame(int id, const std::vector<uint8_t>& data, int compressionThreshold, int compressionLevel) {
	std::vector<uint8_t> frame;
	appendFrame(frame, id, data.data(), data.size(), compressionThreshold, compressionLevel);
	return frame;
}

// Written straight into out: no intermediate buffer unless the packet is compressed
void Packet::appendFrame(std::vector<uint8_t>& out, int id, const uint8_t* data, size_t size, int compressionThreshold, int compressionLevel) {
	size_t bodySize = varintLen(id) + size;
	if (compressionThreshold < 0) {
		out.reserve(out.size() + 5 + bodySize);
		writeVarInt(out, static_cast<uint32_t>(bodySize));
		writeVarInt(out, static_cast<uint32_t>(id));
		out.insert(out.end(), data, data + size);
		return;
	}
	if (static_cast<int64_t>(bodySize) < compressionThreshold) {
		out.reserve(out.size() + 6 + bodySize);
		writeVarInt(out, static_cast<uint32_t>(bodySize + 1));
		out.push_back(0); // Not compressed
		writeVarInt(out, static_cast<uint32_t>(id));
		out.insert(out.end(), data, data + size);
		return;
	}

	t_body.clear();
	writeVarInt(t_body, static_cast<uint32_t>(id));
	t_body.insert(t_body.end(), data, data + size);
	t_compressed.clear();
	compression::zlibCompressAppend(t_body.data(), t_body.size(), compressionLevel, t_compressed);

	out.reserve(out.size() + 10 + t_compressed.size());
	writeVarInt(out, static_cast<uint32_t>(varintLen(static_cast<int>(bodySize)) + t_compressed.size()));
	writeVarInt(out, static_cast<uint32_t>(bodySize));
	out.insert(out.end(), t_compressed.begin(), t_compressed.end());
}

Player*	 Packet::getPlayer() const { return (_player.get()); }
uint32_t Packet::getSize() { return (_size); }
uint32_t Packet::getId() { return (_id); }
Buffer&	 Packet::getData() { return (_data); }
int		 Packet::getSocket() const { return (_socketFd); };
void	 Packet::setPacketSize(int32_t value) { _size = value; }
void	 Packet::setPacketId(uint32_t value) { _id = value; }
