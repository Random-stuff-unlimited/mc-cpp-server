#ifndef CHUNK_STREAMER_HPP
#define CHUNK_STREAMER_HPP

#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Chunk;
class Player;
class Server;

// Sends a player the chunks around them and keeps them loaded while they are in view.
//
// Chunks are sent nearest first, in batches (Chunk Batch Start / Finished). The client acknowledges each batch with
// the rate it can handle; at most MAX_BATCHES_IN_FLIGHT unacknowledged batches are outstanding, so a slow client
// isn't flooded. Chunks leaving the view distance are released and forgotten client-side.
class ChunkStreamer {
  public:
	ChunkStreamer(Server& server, Player& player);
	~ChunkStreamer();

	void start(double x, double z, int viewDistance);
	void onPlayerMove(double x, double z);
	void onBatchReceived(float chunksPerTick);
	// Releases every chunk. Called when the player disconnects
	void stop();
	// Whether the client has this chunk, so it must be told when it changes
	bool hasChunk(int chunkX, int chunkZ);

  private:
	static constexpr int MAX_BATCHES_IN_FLIGHT = 4;
	static constexpr int MIN_BATCH_SIZE		   = 4;
	static constexpr int MAX_BATCH_SIZE		   = 64;

	Server& _server;
	Player& _player;

	std::mutex												 _mutex;
	bool													 _active		  = false;
	int														 _centerX		  = 0;
	int														 _centerZ		  = 0;
	int														 _viewDistance	  = 0;
	int														 _batchesInFlight = 0;
	int														 _batchSize		  = 16;
	std::unordered_set<int64_t>								 _inView; // Chunks we hold a ticket for
	std::unordered_map<int64_t, std::shared_ptr<Chunk>> _ready;	 // Loaded, not sent yet
	std::unordered_set<int64_t>								 _sent;

	void onChunkLoaded(const std::shared_ptr<Chunk>& chunk);
	void sendBatchesLocked();
	void acquire(const std::vector<int64_t>& keys);
	void release(const std::vector<int64_t>& keys);
	std::vector<int64_t> viewAround(int centerX, int centerZ) const;
};

#endif
