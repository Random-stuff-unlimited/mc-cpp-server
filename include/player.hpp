#ifndef PLAYER_HPP
# define PLAYER_HPP

# include <iostream>
# include "enums.hpp"

class Player
{
	private:
		std::string	_name;
		PlayerState _state;
		int			_socket_fd;
		int				x, y, z;
		int				health;

	public:
		Player();
		Player(const std::string &name, const PlayerState state, const int socket);
		Player(const Player &src);
		Player &operator=(const Player &src);
		~Player();

		std::string	getPlayerName(void);
		void		setPlayerName(const std::string &name);
};

#endif