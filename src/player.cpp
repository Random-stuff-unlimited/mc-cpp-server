#include "player.hpp"

Player::Player() : _name("Player_entity"), _state(PlayerState::Ping), _socket_fd(-1), x(0), y(0), z(0), health(0) {}

Player::Player(const std::string &name, const PlayerState state, const int socket)
	: _state(state), _socket_fd(socket), x(0), y(0), z(0), health(20)
{
	if (name.length() > 32)
		_name = name.substr(0, 31);
	else
		_name = name;
}

Player::Player(const Player &src) {*this = src;}

Player	&Player::operator=(const Player &src)
{
	if (this != &src)
	{
		this->_name = src._name;
		this->_socket_fd = src._socket_fd;
		this->health = src.health;
		this->x = src.x;
		this->y = src.y;
		this->z = src.z;
	}
	return (*this);
}

Player::~Player() {}

std::string	Player::getPlayerName(void) {return (this->_name);};
void	Player::setPlayerName(const std::string &name) {this->_name = name;}
int		Player::getSocketFd(void) const {return this->_socket_fd;}