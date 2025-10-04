#include "UUID.hpp"
#include "player.hpp"
#include "server.hpp"

#include <string>

Player::Player(Server& server)
    : _name("Player_entity"), _state(PlayerState::None), _socketFd(-1), x(0), y(0), z(0), health(0),
      _uuid(), _playerId(server.getIdManager().allocate()), _server(server) {}

Player::Player(const std::string& name, const PlayerState state, const int socket, Server& server)
    : _state(state), _socketFd(socket), x(0), y(0), z(0), health(20), _uuid(),
      _playerId(server.getIdManager().allocate()), _server(server) {
	if (name.length() > 32)
		_name = name.substr(0, 31);
	else
		_name = name;
}

Player& Player::operator=(const Player& src) {
	if (this != &src) {
		this->_name     = src._name;
		this->_socketFd = src._socketFd;
		this->health    = src.health;
		this->x         = src.x;
		this->y         = src.y;
		this->z         = src.z;
	}
	return (*this);
}

Player::~Player() {
	_server.getIdManager().release(_playerId);
}

std::string Player::getPlayerName(void) {
	return (this->_name);
};
void Player::setPlayerName(const std::string& name) {
	this->_name = name;
}
PlayerState Player::getPlayerState() {
	return (this->_state);
}
void Player::setPlayerState(PlayerState state) {
	this->_state = state;
}
void Player::setSocketFd(int socket) {
	this->_socketFd = socket;
}
int Player::getSocketFd() const {
	return (this->_socketFd);
}

void Player::setUUID(UUID uuid) {
	_uuid = uuid;
}

int Player::getPlayerID() const {
	return (_playerId);
}
