#include "lib/compression.hpp"
#include "logger.hpp"
#include "network/networking.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <errno.h>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {
	constexpr size_t  READ_CHUNK_SIZE		= 65536;
	constexpr int32_t MAX_UNCOMPRESSED_SIZE = 8388608; // Protocol limit for a decompressed packet
	constexpr int	  MAX_EVENTS			= 256;
	constexpr int	  EPOLL_TIMEOUT_MS		= 1000;	   // Stalled outputs are checked at least this often

	enum VarIntResult { VARINT_OK, VARINT_INCOMPLETE, VARINT_INVALID };

	// Decodes a VarInt from data[pos, end) using at most maxBytes bytes
	VarIntResult decodeVarInt(const uint8_t* data, size_t pos, size_t end, size_t maxBytes, int32_t& value, size_t& bytesRead) {
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

	// Decompressed packets, reused by each network thread
	thread_local std::vector<uint8_t> t_decompressed;
} // namespace

void NetworkManager::threadLoop(NetworkThread& thread) {
	epoll_event events[MAX_EVENTS];
	auto		lastStallCheck = std::chrono::steady_clock::now();

	while (!_shutdownFlag.load()) {
		int eventCount = epoll_wait(thread.epollFd, events, MAX_EVENTS, EPOLL_TIMEOUT_MS);
		if (eventCount == -1) {
			if (errno == EINTR) continue;
			g_logger->logNetwork(ERROR, "epoll_wait failed: " + std::string(strerror(errno)), "Network Manager");
			break;
		}

		for (int i = 0; i < eventCount; i++) {
			int		 fd			= events[i].data.fd;
			uint32_t eventFlags = events[i].events;

			if (fd == _serverSocket) {
				acceptConnections(thread);
				continue;
			}
			if (fd == thread.wakeFd) {
				uint64_t count;
				(void)!read(thread.wakeFd, &count, sizeof(count));
				continue;
			}

			auto it = thread.connections.find(fd);
			if (it == thread.connections.end()) continue; // Closed by an earlier event of this batch
			Connection& connection = it->second;

			bool keep = true;
			if (eventFlags & EPOLLIN) {
				keep = readFromSocket(thread, connection);
			} else if (eventFlags & (EPOLLERR | EPOLLHUP)) {
				keep = false;
			}
			if (keep && (eventFlags & EPOLLOUT)) keep = writeConnection(thread, fd, connection);
			if (!keep) closeConnection(thread, fd);
		}

		// Answers to the Login and Configuration packets just handled, then everything queued for this thread
		flush();
		writeQueued(thread);

		auto now = std::chrono::steady_clock::now();
		if (now - lastStallCheck >= std::chrono::seconds(1)) {
			lastStallCheck = now;
			checkStalledOutputs(thread);
		}
	}
}

// Returns false when the connection must be closed
bool NetworkManager::readFromSocket(NetworkThread& thread, Connection& connection) {
	(void)thread;
	static thread_local uint8_t chunk[READ_CHUNK_SIZE];

	ssize_t bytesRead = ::read(connection.player->getSocketFd(), chunk, sizeof(chunk));
	if (bytesRead == 0) return false; // Peer closed the connection
	if (bytesRead < 0) return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
	if (connection.player->isDisconnected()) return true; // Closing: what it sends doesn't matter anymore

	connection.in.insert(connection.in.end(), chunk, chunk + bytesRead);
	return extractPackets(connection);
}

// Turns every complete frame in the receive buffer into a Packet. Returns false on a malformed or oversized frame
bool NetworkManager::extractPackets(Connection& connection) {
	const std::shared_ptr<Player>& player = connection.player;
	int							   fd	  = player->getSocketFd();
	std::vector<uint8_t>&		   buf	  = connection.in;
	size_t						   pos	  = connection.inStart;

	while (!player->isDisconnected()) {
		int32_t		 length		 = 0;
		size_t		 lengthBytes = 0;
		VarIntResult res		 = decodeVarInt(buf.data(), pos, buf.size(), 3, length, lengthBytes);
		if (res == VARINT_INCOMPLETE) break;
		if (res == VARINT_INVALID || length <= 0 || length > MAX_PACKET_SIZE) {
			g_logger->logNetwork(WARN, "Invalid packet length from socket " + std::to_string(fd), "Network Manager");
			return false;
		}

		size_t frameStart = pos + lengthBytes;
		if (buf.size() - frameStart < static_cast<size_t>(length)) break; // Wait for the rest of the frame
		size_t frameEnd = frameStart + length;

		// After Set Compression, a frame is: VarInt uncompressed size (0 = not compressed), then id + payload
		const uint8_t* body		 = buf.data();
		size_t		   bodyStart = frameStart;
		size_t		   bodyEnd	 = frameEnd;
		if (player->getCompressionThreshold() >= 0) {
			int32_t dataLength		= 0;
			size_t	dataLengthBytes = 0;
			if (decodeVarInt(buf.data(), frameStart, frameEnd, 5, dataLength, dataLengthBytes) != VARINT_OK || dataLength < 0 ||
				dataLength > MAX_UNCOMPRESSED_SIZE) {
				g_logger->logNetwork(WARN, "Invalid compressed packet from socket " + std::to_string(fd), "Network Manager");
				return false;
			}
			bodyStart = frameStart + dataLengthBytes;
			if (dataLength > 0) {
				t_decompressed.resize(dataLength);
				if (!compression::zlibDecompressInto(buf.data() + bodyStart, frameEnd - bodyStart, t_decompressed.data(), dataLength)) {
					g_logger->logNetwork(WARN, "Corrupted compressed packet from socket " + std::to_string(fd), "Network Manager");
					return false;
				}
				body	  = t_decompressed.data();
				bodyStart = 0;
				bodyEnd	  = dataLength;
			}
		}

		int32_t id		= 0;
		size_t	idBytes = 0;
		if (decodeVarInt(body, bodyStart, bodyEnd, 5, id, idBytes) != VARINT_OK) {
			g_logger->logNetwork(WARN, "Invalid packet id from socket " + std::to_string(fd), "Network Manager");
			return false;
		}

		pos = frameEnd;
		dispatch(new Packet(player, id, std::vector<uint8_t>(body + bodyStart + idBytes, body + bodyEnd), length));
	}

	// Keep the unread bytes; move them to the front only once they are a good part of the buffer
	if (pos == buf.size()) {
		buf.clear();
		pos = 0;
	} else if (pos > buf.size() / 2) {
		buf.erase(buf.begin(), buf.begin() + pos);
		pos = 0;
	}
	connection.inStart = pos;
	return true;
}

// Takes the output of the players queued for this thread and writes it
void NetworkManager::writeQueued(NetworkThread& thread) {
	std::vector<std::shared_ptr<Player>> players;
	{
		std::lock_guard<std::mutex> lock(thread.flushMutex);
		if (thread.toFlush.empty()) return;
		players.swap(thread.toFlush);
	}
	for (const auto& player : players) {
		// Cleared before taking the output: what is appended after this is queued again
		player->output().flushQueued = false;
		int	 fd = player->getSocketFd();
		auto it = thread.connections.find(fd);
		if (it == thread.connections.end() || it->second.player != player) continue; // Already closed

		Connection& connection = it->second;
		bool		closing;
		{
			std::lock_guard<std::mutex> lock(player->output().mutex);
			std::vector<uint8_t>&		data = player->output().data;
			if (connection.outOffset == connection.out.size()) {
				// Nothing pending: take the buffer as is, and give ours back for the next packets
				connection.out.swap(data);
				connection.outOffset = 0;
			} else {
				connection.out.insert(connection.out.end(), data.begin(), data.end());
			}
			data.clear();
			closing = player->output().closing;
		}
		if (!connection.waitingWritable && !writeConnection(thread, fd, connection)) {
			closeConnection(thread, fd);
			continue;
		}
		if (closing && connection.outOffset == connection.out.size()) closeConnection(thread, fd);
	}
}

// Writes what the socket accepts without blocking; the rest waits for EPOLLOUT. Returns false if the connection is
// broken or the client too slow
bool NetworkManager::writeConnection(NetworkThread& thread, int fd, Connection& connection) {
	std::vector<uint8_t>& out = connection.out;
	while (connection.outOffset < out.size()) {
		ssize_t n = send(fd, out.data() + connection.outOffset, out.size() - connection.outOffset, MSG_NOSIGNAL);
		if (n > 0) {
			connection.outOffset += n;
			connection.lastProgress = std::chrono::steady_clock::now();
			continue;
		}
		if (n < 0 && errno == EINTR) continue;
		if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
		return false;
	}

	size_t left = out.size() - connection.outOffset;
	if (left > MAX_PENDING_OUTPUT) return false; // Dropping it is better than buffering without limit
	if (left == 0) {
		out.clear();
		connection.outOffset = 0;
		if (out.capacity() > MAX_PENDING_OUTPUT / 4) std::vector<uint8_t>().swap(out); // After a burst (chunks)
		if (connection.waitingWritable) watchWritable(thread, fd, connection, false);
		// Closing after the last bytes
		std::lock_guard<std::mutex> lock(connection.player->output().mutex);
		return !(connection.player->output().closing && connection.player->output().data.empty());
	}
	if (!connection.waitingWritable) watchWritable(thread, fd, connection, true);
	return true;
}

void NetworkManager::watchWritable(NetworkThread& thread, int fd, Connection& connection, bool writable) {
	epoll_event event{};
	event.events  = writable ? EPOLLIN | EPOLLOUT : EPOLLIN;
	event.data.fd = fd;
	epoll_ctl(thread.epollFd, EPOLL_CTL_MOD, fd, &event);
	connection.waitingWritable = writable;
}

void NetworkManager::checkStalledOutputs(NetworkThread& thread) {
	auto			 now = std::chrono::steady_clock::now();
	std::vector<int> stalled;
	for (const auto& [fd, connection] : thread.connections) {
		if (connection.outOffset < connection.out.size() && now - connection.lastProgress > PENDING_OUTPUT_TIMEOUT) stalled.push_back(fd);
	}
	for (int fd : stalled) closeConnection(thread, fd);
}

// Network thread of the connection only
void NetworkManager::closeConnection(NetworkThread& thread, int fd) {
	auto it = thread.connections.find(fd);
	if (it == thread.connections.end()) return;
	std::shared_ptr<Player> player = std::move(it->second.player);
	thread.connections.erase(it);

	requestDisconnect(player.get()); // Nothing if it was requested already
	epoll_ctl(thread.epollFd, EPOLL_CTL_DEL, fd, nullptr);
	// The socket number can be reused from here: the player is out of every list already
	close(fd);
}
