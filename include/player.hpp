#ifndef PLAYER_HPP
# define PLAYER_HPP

# include <iostream>
# include "enums.hpp"
# include "UUID.hpp"

class Player
{
	private:
		std::string	_name;
		PlayerState _state;
		int			_socketFd;
		int			x, y, z;
		int			health;
		UUID		_uuid;
	public:
		Player();
		Player(const std::string &name, const PlayerState state, const int socket);
		Player &operator=(const Player &src);
		~Player();

		const std::string	getPlayerName(void);
		const void		setPlayerName(const std::string &name);
		const PlayerState	getPlayerState();
		const void	setSocketFd(int socket);
		const int	getSocketFd();
};

#endif