#ifndef SPECTATOR_CAMERA_HPP
#define SPECTATOR_CAMERA_HPP

class Actor;
class Player;
class Server;

// ServerPlayer.setCamera / getCamera: a spectator that hits an entity sees through its eyes (Set Camera) and follows
// it, until it sneaks, the entity is gone, or it isn't a spectator anymore
namespace SpectatorCamera {
	// The entity to watch from, or null for the player itself
	void setCamera(Server& server, Player& player, Actor* target);
	// ServerPlayer.tick: stuck to the watched entity, or back to itself
	void tick(Server& server, Player& player);
} // namespace SpectatorCamera

#endif
