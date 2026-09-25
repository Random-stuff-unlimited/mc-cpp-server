#ifndef PLAYER_TRACKER_HPP
#define PLAYER_TRACKER_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Buffer;
class Player;
class Server;

// Makes players see each other: the tab list (everyone in game) and the player entities within view distance.
//
// A player is shown to another when it is in a chunk the other one has in view (square view distance, like the
// chunks). Visibility is only recomputed when a player changes chunk.
//
// Like vanilla, movements aren't forwarded as they arrive: every UPDATE_INTERVAL ticks, each player that moved is
// encoded once (small relative moves when possible), and each viewer receives the movements of all the players it
// sees in a single append. Players nobody sees cost nothing.
//
// Players are indexed in a grid of cells of chunks: a player changing chunk is only compared with the players of the
// cells within view distance, not with everyone.
//
// Game thread only.
class PlayerTracker {
  public:
	explicit PlayerTracker(Server& server);

	void join(const std::shared_ptr<Player>& player, int viewDistance);
	void leave(Player* player);
	// After the player's position and/or rotation changed: sent at the next update
	void move(Player* player);
	// Every tick: visibility of the players that changed chunk, and every UPDATE_INTERVAL ticks their movements
	void tick(int64_t tickCount);
	// Removes and spawns the player again for the others, at its current position (after respawning)
	void respawn(Player* player);

	// The player with this entity id, if `viewer` can see it (attacks can only target visible players)
	std::shared_ptr<Player> findVisible(Player* viewer, int entityId);
	// Chat message to everyone in game, e.g. ("multiplayer.player.joined", {"Bob"}, "yellow")
	void broadcastMessage(const std::string& translationKey, const std::vector<std::string>& args, const std::string& color);

	// Sends a packet to the players that see `subject`, and to `subject` itself if includeSubject
	void broadcast(Player* subject, int packetId, Buffer& data, bool includeSubject);

  private:
	struct Tracked {
		std::shared_ptr<Player>		player;
		int							viewDistance;
		int							chunkX, chunkZ;
		std::unordered_set<Player*> viewers;			  // Players this one is shown to
		std::unordered_set<Player*> visible;			  // Players shown to this one
		int64_t						sentX, sentY, sentZ; // Last position sent, in 1/4096 of a block
		uint8_t						sentYaw, sentPitch;
		int64_t						lastSync = 0; // Tick of the last absolute position sent
		bool						moved	 = false; // In _moved
		std::vector<uint8_t>		update;			  // Movement packets of this update, for all the viewers
		std::vector<const Tracked*> pending;		  // Movers whose update this viewer gets
	};

	static constexpr int	 CELL_SHIFT		  = 3;	 // Cells of 8x8 chunks
	static constexpr int64_t UPDATE_INTERVAL  = 2;	 // Ticks, vanilla's for players
	static constexpr int64_t FORCE_SYNC_TICKS = 400; // Absolute position at least this often while moving

	Server&											  _server;
	std::unordered_map<Player*, Tracked>			  _players;
	std::unordered_map<int64_t, std::vector<Player*>> _cells; // Players by cell
	std::unordered_map<int, Player*>				  _byEntityId;
	int												  _maxViewDistance = 0; // Of all the players that joined
	std::vector<Player*>							  _moved;				// Since the last update
	std::vector<Tracked*>							  _updated;
	std::vector<Tracked*>							  _receivers;

	static int64_t cellKey(int chunkX, int chunkZ);
	void		   addToCell(const Tracked& tracked);
	void		   removeFromCell(const Tracked& tracked);
	bool		   canSee(const Tracked& viewer, const Tracked& target) const;
	// Updates visibility between `subject` and the players it may concern: the ones near it, and the ones it sees
	// or is seen by
	void updateVisibilityAround(Tracked& subject);
	void updateVisibility(Tracked& a, Tracked& b);
	void encodeMovement(Tracked& tracked, int64_t tickCount);
	void show(Tracked& target, Tracked& viewer);
	void hide(Tracked& target, Tracked& viewer);
	void sendToViewers(const Tracked& target, int packetId, Buffer& data);
	void sendFrameToViewers(const Tracked& target, const std::vector<uint8_t>& frame);
	static void writePlayerInfo(Buffer& info, const std::vector<Player*>& players);
	void		sendPlayerInfo(const std::shared_ptr<Player>& to, const std::vector<Player*>& players);
};

#endif
