#ifndef PLAYER_TRACKER_HPP
#define PLAYER_TRACKER_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Buffer;
class Player;
class Server;

// Makes players see each other: the tab list (everyone in game) and the player entities within view distance.
//
// A player is shown to another when it is in a chunk the other one has in view (square view distance, like the
// chunks). Visibility is only recomputed when a player changes chunk; movements are sent to the players that see
// the mover, as small relative moves when possible. Each packet is encoded once for all of them.
class PlayerTracker {
  public:
	explicit PlayerTracker(Server& server);

	void join(const std::shared_ptr<Player>& player, int viewDistance);
	void leave(Player* player);
	// After the player's position and/or rotation changed
	void move(Player* player, bool positionChanged, bool rotationChanged);
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
		int64_t						sentX, sentY, sentZ; // Last position sent, in 1/4096 of a block
		uint8_t						sentYaw, sentPitch;
	};

	Server&								 _server;
	std::mutex							 _mutex;
	std::unordered_map<Player*, Tracked> _players;

	bool canSee(const Tracked& viewer, const Tracked& target) const;
	void updateVisibility(Tracked& a, Tracked& b);
	void show(Tracked& target, Tracked& viewer);
	void sendToViewers(const Tracked& target, int packetId, Buffer& data);
	void sendPlayerInfo(const std::shared_ptr<Player>& to, const std::vector<Player*>& players);
	void sendMessageLocked(const std::string& translationKey, const std::vector<std::string>& args, const std::string& color);

};

#endif
