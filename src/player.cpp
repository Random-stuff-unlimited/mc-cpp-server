#include "player.hpp"

Player::Player() : _name("Player_entity"), _state(PlayerState::Ping), _socketFd(-1), x(0), y(0), z(0), health(0), _uuid() {
	
}

Player::Player(const std::string &name, const PlayerState state, const int socket)
{
	if (name.length() > 32)
		_name = name.substr(0, 31);
	else
		_name = name;
}

Player	&Player::operator=(const Player &src)
{
	if (this != &src)
	{
		this->_name = src._name;
		this->_socketFd = src._socketFd;
		this->health = src.health;
		this->x = src.x;
		this->y = src.y;
		this->z = src.z;
	}
	return (*this);
}

Player::~Player() {}

const std::string	Player::getPlayerName(void) {return (this->_name);};
const void	Player::setPlayerName(const std::string &name) {this->_name = name;}
const PlayerState	Player::getPlayerState() {return (this->getPlayerState());}
const void	Player::setSocketFd(int socket) {this->_socketFd = socket;}
const int	Player::getSocketFd() {return (this->_socketFd);}