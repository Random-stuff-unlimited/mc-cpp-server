#include "lib/compression.hpp"
#include "logger.hpp"
#include "network/networking.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/ChunkStreamer.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <cstdint>
#include <errno.h>
#include <exception>
#include <netinet/in.h>
#include <poll.h>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {
	constexpr size_t READ_CHUNK_SIZE = 65536;
	constexpr int32_t MAX_UNCOMPRESSED_SIZE = 8388608; // Protocol limit for a decompressed packet

	enum VarIntResult { VARINT_OK, VARINT_INCOMPLETE, VARINT_INVALID };

	// Decodes a VarInt from data[pos, end) using at most maxBytes bytes
	VarIntResult decodeVarInt(const std::vector<uint8_t>& data, size_t pos, size_t end, size_t maxBytes, int32_t& value, size_t& bytesRead) {
		uint32_t result = 0;
		for (size_t i = 0; i < maxBytes; i++) {
			if (pos + i >= end) return VARINT_INCOMPLETE;
			uint8_t byte = data[pos + i];
			result |= static_cast<uint32_t>(byte & 0x7F) << (7 * i);
			if (!(byte & 0x80)) {
				value	  = static_cast<int32_t>(result);
				bytesRead = i + 1;
				return VARINT_OK;
			}
		}
		return VARINT_INVALID;
	}
} // namespace

void NetworkManager::receiverThreadLoop() {
	const int	MaxEvent = 256;
	epoll_event events[MaxEvent];

	while (!_shutdownFlag.load()) {
		int eventCount = epoll_wait(_epollFd, events, MaxEvent, 50);

		if (eventCount == -1) {
			if (errno == EINTR) continue;
			break;
		}

		for (int i = 0; i < eventCount; i++) {
			int		 fd			= events[i].data.fd;
			uint32_t eventFlags = events[i].events;

			if (fd == _serverSocket) {
				acceptConnections();
				continue;
			}

			// Unknown or disconnecting player: stale event for a socket that was just detached
			std::shared_ptr<Player> player = getServer().findPlayer(fd);
			if (!player || player->isDisconnected()) {
				_recvBuffers.erase(fd);
				continue;
			}

			bool keepConnection = true;
			if (eventFlags & EPOLLIN) {
				keepConnection = readFromSocket(player);
			} else if (eventFlags & (EPOLLERR | EPOLLHUP)) {
				keepConnection = false;
			}

			if (!keepConnection) {
				_recvBuffers.erase(fd);
				requestDisconnect(player.get());
			}
		}
	}
}

void NetworkManager::acceptConnections() {
	// The server socket is edge-triggered: accept until the backlog is empty
	while (true) {
		sockaddr_in clientAddr{};
		socklen_t	addrLen	 = sizeof(clientAddr);
		int			clientFd = accept4(_serverSocket, (sockaddr*)&clientAddr, &addrLen, SOCK_NONBLOCK | SOCK_CLOEXEC);
		if (clientFd == -1) {
			if (errno == EINTR) continue;
			return;
		}

		try {
			getServer().addTempPlayer("None", PlayerState::Handshake, clientFd);
		} catch (const std::exception& e) {
			g_logger->logNetwork(ERROR, "Failed to create player for new connection: " + std::string(e.what()), "Network Manager");
			close(clientFd);
			continue;
		}
		// The socket number may have been used by a previous connection
		_recvBuffers[clientFd].clear();

		epoll_event event{};
		event.events  = EPOLLIN;
		event.data.fd = clientFd;
		if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, clientFd, &event) == -1) {
			g_logger->logNetwork(ERROR, "Failed to add new client socket to epoll", "Network Manager");
			std::shared_ptr<Player> player = getServer().findPlayer(clientFd);
			if (player) getServer().removePlayerFromAnyList(player.get());
			_recvBuffers.erase(clientFd);
			close(clientFd);
		}
	}
}

// Returns false when the connection must be closed
bool NetworkManager::readFromSocket(const std::shared_ptr<Player>& player) {
	int					  fd  = player->getSocketFd();
	std::vector<uint8_t>& buf = _recvBuffers[fd];

	uint8_t chunk[READ_CHUNK_SIZE];
	ssize_t bytesRead = ::read(fd, chunk, sizeof(chunk));
	if (bytesRead == 0) return false; // Peer closed the connection
	if (bytesRead < 0) return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;

	buf.insert(buf.end(), chunk, chunk + bytesRead);
	return extractPackets(player);
}

// Turns every complete frame in the receive buffer into a Packet. Returns false on a malformed or oversized frame
bool NetworkManager::extractPackets(const std::shared_ptr<Player>& player) {
	int					  fd  = player->getSocketFd();
	std::vector<uint8_t>& buf = _recvBuffers[fd];
	size_t				  pos = 0;

	while (true) {
		int32_t		 length		 = 0;
		size_t		 lengthBytes = 0;
		VarIntResult res		 = decodeVarInt(buf, pos, buf.size(), 3, length, lengthBytes);
		if (res == VARINT_INCOMPLETE) break;
		if (res == VARINT_INVALID || length <= 0 || length > MAX_PACKET_SIZE) {
			g_logger->logNetwork(WARN, "Invalid packet length from socket " + std::to_string(fd), "Network Manager");
			return false;
		}

		size_t frameStart = pos + lengthBytes;
		if (buf.size() - frameStart < static_cast<size_t>(length)) break; // Wait for the rest of the frame
		size_t frameEnd = frameStart + length;

		// After Set Compression, a frame is: VarInt uncompressed size (0 = not compressed), then id + payload
		std::vector<uint8_t> decompressed;
		const std::vector<uint8_t>* body	  = &buf;
		size_t						bodyStart = frameStart;
		size_t						bodyEnd	  = frameEnd;
		if (player->getCompressionThreshold() >= 0) {
			int32_t dataLength		= 0;
			size_t	dataLengthBytes = 0;
			if (decodeVarInt(buf, frameStart, frameEnd, 5, dataLength, dataLengthBytes) != VARINT_OK || dataLength < 0 ||
				dataLength > MAX_UNCOMPRESSED_SIZE) {
				g_logger->logNetwork(WARN, "Invalid compressed packet from socket " + std::to_string(fd), "Network Manager");
				return false;
			}
			bodyStart = frameStart + dataLengthBytes;
			if (dataLength > 0) {
				try {
					decompressed = compression::zlibDecompress(buf.data() + bodyStart, frameEnd - bodyStart, dataLength);
				} catch (const std::exception&) {
					g_logger->logNetwork(WARN, "Corrupted compressed packet from socket " + std::to_string(fd), "Network Manager");
					return false;
				}
				body	  = &decompressed;
				bodyStart = 0;
				bodyEnd	  = decompressed.size();
			}
		}

		int32_t id		= 0;
		size_t	idBytes = 0;
		if (decodeVarInt(*body, bodyStart, bodyEnd, 5, id, idBytes) != VARINT_OK) {
			g_logger->logNetwork(WARN, "Invalid packet id from socket " + std::to_string(fd), "Network Manager");
			return false;
		}

		std::vector<uint8_t> payload(body->begin() + bodyStart + idBytes, body->begin() + bodyEnd);
		_workerQueues[fd % _workerCount]->push(new Packet(player, id, payload, length));
		pos = frameEnd;
	}

	buf.erase(buf.begin(), buf.begin() + pos);
	return true;
}

// Sockets are non-blocking: what a client can't take right away waits in its own output buffer,
// so a slow client never delays the others
void NetworkManager::senderThreadLoop() {
	bool running = true;
	while (running && !_shutdownFlag.load()) {
		Packet* p	 = nullptr;
		auto	wait = _pendingOutput.empty() ? std::chrono::milliseconds(100) : std::chrono::milliseconds(2);
		if (_outgoingPackets.waitAndPopTimeout(p, wait)) {
			do {
				if (p == nullptr) {
					running = false;
					break;
				}
				queueOutput(p);
			} while (_outgoingPackets.tryPop(p));
		}
		flushPendingOutput();
	}
}

void NetworkManager::queueOutput(Packet* p) {
	Player* player = p->getPlayer();
	if (!player || player->isSocketClosed()) {
		delete p;
		return;
	}
	auto pending = _pendingOutput.find(player);

	// A disconnect marker is queued after the player's last packets: close once they are sent
	if (p->getReturnPacket() == PACKET_DISCONNECT) {
		if (pending == _pendingOutput.end()) {
			closePlayerSocket(player);
		} else {
			pending->second.closeWhenFlushed = true;
		}
		delete p;
		return;
	}

	const std::vector<uint8_t>& data = p->getData().getData();
	if (pending == _pendingOutput.end()) {
		size_t sent = 0;
		if (!sendAvailable(player->getSocketFd(), data.data(), data.size(), sent)) {
			closePlayerSocket(player);
		} else if (sent < data.size()) {
			PendingOutput& output = _pendingOutput[player];
			output.player		  = player->shared_from_this();
			output.data.assign(data.begin() + sent, data.end());
			output.lastProgress = std::chrono::steady_clock::now();
		}
	} else {
		pending->second.data.insert(pending->second.data.end(), data.begin(), data.end());
	}
	delete p;
}

void NetworkManager::flushPendingOutput() {
	auto now = std::chrono::steady_clock::now();
	for (auto it = _pendingOutput.begin(); it != _pendingOutput.end();) {
		PendingOutput& output = it->second;
		Player*		   player = output.player.get();
		size_t		   sent	  = 0;
		bool		   ok	  = sendAvailable(player->getSocketFd(), output.data.data() + output.offset, output.data.size() - output.offset, sent);
		if (sent > 0) {
			output.offset += sent;
			output.lastProgress = now;
		}

		size_t left = output.data.size() - output.offset;
		if (!ok || left > MAX_PENDING_OUTPUT || now - output.lastProgress > PENDING_OUTPUT_TIMEOUT) {
			// Dead or too slow: dropping it is better than buffering without limit
			it = _pendingOutput.erase(it);
			closePlayerSocket(player);
			continue;
		}
		if (left == 0) {
			bool close = output.closeWhenFlushed;
			it		   = _pendingOutput.erase(it);
			if (close) closePlayerSocket(player);
			continue;
		}
		if (output.offset > (1 << 20)) {
			output.data.erase(output.data.begin(), output.data.begin() + output.offset);
			output.offset = 0;
		}
		++it;
	}
}

// Sends what the socket accepts without blocking. Returns false if the connection is broken
bool NetworkManager::sendAvailable(int socket, const uint8_t* data, size_t size, size_t& sent) {
	sent = 0;
	while (sent < size) {
		ssize_t n = send(socket, data + sent, size - sent, MSG_NOSIGNAL);
		if (n > 0) {
			sent += n;
			continue;
		}
		if (n < 0 && errno == EINTR) continue;
		return n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK);
	}
	return true;
}

void NetworkManager::enqueueOutgoingPacket(Packet* p) { _outgoingPackets.push(p); }

// Removes the player from the server and stops polling its socket. Returns false if it was already done
bool NetworkManager::detachPlayer(Player* player) {
	if (!player->markDisconnected()) return false;
	if (ChunkStreamer* streamer = player->getChunkStreamer()) streamer->stop();
	getServer().getPlayerTracker().leave(player);
	getServer().removePlayerFromAnyList(player);
	epoll_ctl(_epollFd, EPOLL_CTL_DEL, player->getSocketFd(), nullptr);
	return true;
}

void NetworkManager::requestDisconnect(Player* player) {
	if (!player || !detachPlayer(player)) return;

	// Only the sender thread closes sockets, so a socket number can't be reused while packets for it are still queued
	Packet* marker = new Packet(player->shared_from_this());
	marker->setReturnPacket(PACKET_DISCONNECT);
	_outgoingPackets.push(marker);
}

// Sender thread only
void NetworkManager::closePlayerSocket(Player* player) {
	if (player->isSocketClosed()) return;
	_pendingOutput.erase(player);
	detachPlayer(player);
	close(player->getSocketFd());
	player->setSocketClosed();
}
