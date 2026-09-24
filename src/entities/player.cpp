#include "player.hpp"

#include "lib/UUID.hpp"
#include "network/server.hpp"
#include "world/ChunkStreamer.hpp"

#include <string>

Player::Player(Server& server)
	: _name("Player_entity"), _state(PlayerState::None), _socketFd(-1), _disconnected(false), _socketClosed(false), x(0), y(0), z(0), health(0), _uuid(),
	  _playerId(server.getIdManager().allocate()), _server(server), _config(new PlayerConfig()) {
	_inventory.fill(-1);
}

Player::Player(const std::string& name, const PlayerState state, const int socket, Server& server)
	: _state(state), _socketFd(socket), _disconnected(false), _socketClosed(false), x(0), y(0), z(0), health(20), _uuid(), _playerId(server.getIdManager().allocate()), _server(server),
	  _config(new PlayerConfig()) {
	_inventory.fill(-1);
	if (name.length() > 32)
		_name = name.substr(0, 31);
	else
		_name = name;
}

Player& Player::operator=(const Player& src) {
	if (this != &src) {
		this->_name		= src._name;
		this->_socketFd = src._socketFd;
		this->health	= src.health;
		this->x			= src.x;
		this->y			= src.y;
		this->z			= src.z;
	}
	return (*this);
}

Player::~Player() {
	_chunkStreamer.reset(); // Releases its chunks while the player is still whole
	_server.getIdManager().release(_playerId);
	delete _config;
}

std::string Player::getPlayerName(void) { return (this->_name); };
void		Player::setPlayerName(const std::string& name) { this->_name = name; }
PlayerState Player::getPlayerState() { return (this->_state); }
void		Player::setPlayerState(PlayerState state) { this->_state = state; }
void		Player::setSocketFd(int socket) { this->_socketFd = socket; }
int			Player::getSocketFd() const { return (this->_socketFd); }

void Player::setUUID(UUID uuid) { _uuid = uuid; }

void Player::createChunkStreamer() { _chunkStreamer = std::make_unique<ChunkStreamer>(_server, *this); }

int Player::getPlayerID() const { return (_playerId); }

// PlayerConfig implementation
PlayerConfig::PlayerConfig()
	: _chatMode(0), _mainHand(1), _locale("en_US"), _viewDistance(10), _displayedSkinParts(0), _chatColors(true), _enableTextFiltering(false),
	  _allowServerListings(true) {}

PlayerConfig::~PlayerConfig() {}
