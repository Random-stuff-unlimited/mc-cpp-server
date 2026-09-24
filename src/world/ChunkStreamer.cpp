#include "world/ChunkStreamer.hpp"

#include "PacketIds.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/World.hpp"

#include <algorithm>
#include <cmath>

namespace {
	int chunkX(int64_t key) { return static_cast<int32_t>(key & 0xFFFFFFFF); }
	int chunkZ(int64_t key) { return static_cast<int32_t>(key >> 32); }
	int toChunk(double coordinate) { return static_cast<int>(std::floor(coordinate / 16.0)); }
} // namespace

ChunkStreamer::ChunkStreamer(Server& server, Player& player) : _server(server), _player(player) {}

ChunkStreamer::~ChunkStreamer() { stop(); }

std::vector<int64_t> ChunkStreamer::viewAround(int centerX, int centerZ, int radius) const {
	std::vector<int64_t> keys;
	keys.reserve((2 * radius + 1) * (2 * radius + 1));
	for (int dz = -radius; dz <= radius; dz++) {
		for (int dx = -radius; dx <= radius; dx++) keys.push_back(Chunk::key(centerX + dx, centerZ + dz));
	}
	// Nearest first: the I/O threads load in submission order
	std::sort(keys.begin(), keys.end(), [centerX, centerZ](int64_t a, int64_t b) {
		int da = (chunkX(a) - centerX) * (chunkX(a) - centerX) + (chunkZ(a) - centerZ) * (chunkZ(a) - centerZ);
		int db = (chunkX(b) - centerX) * (chunkX(b) - centerX) + (chunkZ(b) - centerZ) * (chunkZ(b) - centerZ);
		return da < db;
	});
	return keys;
}

void ChunkStreamer::start(double x, double z, int viewDistance) {
	std::vector<int64_t> tickets;
	std::vector<int64_t> view;
	{
		std::lock_guard<std::mutex> lock(_mutex);
		_active		  = true;
		_centerX	  = toChunk(x);
		_centerZ	  = toChunk(z);
		_viewDistance = viewDistance;

		Buffer center;
		center.writeVarInt(_centerX);
		center.writeVarInt(_centerZ);
		Packet::send(_player.shared_from_this(), PacketId::Play::Clientbound::SET_CHUNK_CACHE_CENTER, center, _server);

		tickets = viewAround(_centerX, _centerZ, _viewDistance + 1);
		view	= viewAround(_centerX, _centerZ, _viewDistance);
		_tickets.insert(tickets.begin(), tickets.end());
		_inView.insert(view.begin(), view.end());
	}
	// World calls happen without our lock: a lit chunk calls onChunkLoaded right away
	acquire(tickets);
	waitForLight(view);
}

void ChunkStreamer::onPlayerMove(double x, double z) {
	std::vector<int64_t> entering, leaving;			  // Tickets
	std::vector<int64_t> enteringView;				  // Chunks to send once lit
	{
		std::lock_guard<std::mutex> lock(_mutex);
		int							newX = toChunk(x);
		int							newZ = toChunk(z);
		if (!_active || (newX == _centerX && newZ == _centerZ)) return;
		_centerX = newX;
		_centerZ = newZ;

		Buffer center;
		center.writeVarInt(_centerX);
		center.writeVarInt(_centerZ);
		Packet::send(_player.shared_from_this(), PacketId::Play::Clientbound::SET_CHUNK_CACHE_CENTER, center, _server);

		std::vector<int64_t>		newTickets = viewAround(_centerX, _centerZ, _viewDistance + 1);
		std::vector<int64_t>		newView	   = viewAround(_centerX, _centerZ, _viewDistance);
		std::unordered_set<int64_t> newTicketSet(newTickets.begin(), newTickets.end());
		std::unordered_set<int64_t> newViewSet(newView.begin(), newView.end());
		for (int64_t key : _tickets) {
			if (!newTicketSet.count(key)) leaving.push_back(key);
		}
		for (int64_t key : leaving) _tickets.erase(key);
		for (int64_t key : newTickets) {
			if (_tickets.insert(key).second) entering.push_back(key);
		}

		std::vector<int64_t> leavingView;
		for (int64_t key : _inView) {
			if (newViewSet.count(key)) continue;
			leavingView.push_back(key);
			_ready.erase(key);
			if (_sent.erase(key)) {
				// ChunkPos as a long: z in the high 32 bits
				Buffer forget;
				forget.writeLong(key);
				Packet::send(_player.shared_from_this(), PacketId::Play::Clientbound::FORGET_LEVEL_CHUNK, forget, _server);
			}
		}
		for (int64_t key : leavingView) _inView.erase(key);
		for (int64_t key : newView) {
			if (_inView.insert(key).second) enteringView.push_back(key);
		}
		// Chunks loaded for the old position may now be further than the new nearest ones
		sendBatchesLocked();
	}
	release(leaving);
	acquire(entering);
	waitForLight(enteringView);
}

void ChunkStreamer::onBatchReceived(float chunksPerTick) {
	std::lock_guard<std::mutex> lock(_mutex);
	_batchesInFlight = std::max(0, _batchesInFlight - 1);
	if (std::isfinite(chunksPerTick)) {
		_batchSize = std::clamp(static_cast<int>(std::ceil(chunksPerTick)), MIN_BATCH_SIZE, MAX_BATCH_SIZE);
	}
	sendBatchesLocked();
}

void ChunkStreamer::stop() {
	std::vector<int64_t> keys;
	{
		std::lock_guard<std::mutex> lock(_mutex);
		if (!_active) return;
		_active = false;
		keys.assign(_tickets.begin(), _tickets.end());
		_tickets.clear();
		_inView.clear();
		_ready.clear();
		_sent.clear();
	}
	release(keys);
}

bool ChunkStreamer::hasChunk(int chunkX, int chunkZ) {
	std::lock_guard<std::mutex> lock(_mutex);
	return _sent.count(Chunk::key(chunkX, chunkZ)) != 0;
}

void ChunkStreamer::onChunkLoaded(const std::shared_ptr<Chunk>& chunk) {
	std::lock_guard<std::mutex> lock(_mutex);
	int64_t						key = Chunk::key(chunk->x(), chunk->z());
	// The player may have moved away while it was loading
	if (!_active || !_inView.count(key) || _sent.count(key)) return;
	_ready[key] = chunk;
	sendBatchesLocked();
}

void ChunkStreamer::sendBatchesLocked() {
	std::shared_ptr<Player> player	  = _player.shared_from_this();
	int						threshold = _server.getConfig().getCompressionThreshold();

	while (_batchesInFlight < MAX_BATCHES_IN_FLIGHT && !_ready.empty()) {
		std::vector<std::pair<int, int64_t>> byDistance;
		byDistance.reserve(_ready.size());
		for (const auto& [key, chunk] : _ready) {
			int dx = chunkX(key) - _centerX;
			int dz = chunkZ(key) - _centerZ;
			byDistance.emplace_back(dx * dx + dz * dz, key);
		}
		size_t count = std::min<size_t>(_batchSize, byDistance.size());
		std::partial_sort(byDistance.begin(), byDistance.begin() + count, byDistance.end());

		Buffer empty;
		Packet::send(player, PacketId::Play::Clientbound::CHUNK_BATCH_START, empty, _server);
		for (size_t i = 0; i < count; i++) {
			int64_t key = byDistance[i].second;
			auto	it	= _ready.find(key);
			Packet::sendFrame(player, *_server.getWorld().getChunkPacket(it->second, threshold), _server);
			_sent.insert(key);
			_ready.erase(it);
		}
		Buffer finished;
		finished.writeVarInt(static_cast<int32_t>(count));
		Packet::send(player, PacketId::Play::Clientbound::CHUNK_BATCH_FINISHED, finished, _server);
		_batchesInFlight++;
	}
}

void ChunkStreamer::acquire(const std::vector<int64_t>& keys) {
	for (int64_t key : keys) _server.getWorld().acquireChunk(chunkX(key), chunkZ(key));
}

// Chunks are sent once lit (their neighbors are loaded then)
void ChunkStreamer::waitForLight(const std::vector<int64_t>& keys) {
	std::weak_ptr<Player> weakPlayer = _player.shared_from_this();
	for (int64_t key : keys) {
		_server.getWorld().whenLit(chunkX(key), chunkZ(key), [weakPlayer](const std::shared_ptr<Chunk>& chunk) {
			std::shared_ptr<Player> player = weakPlayer.lock();
			if (player && player->getChunkStreamer()) player->getChunkStreamer()->onChunkLoaded(chunk);
		});
	}
}

void ChunkStreamer::release(const std::vector<int64_t>& keys) {
	for (int64_t key : keys) _server.getWorld().releaseChunk(chunkX(key), chunkZ(key));
}
