#ifndef SERVER_HPP
# define SERVER_HPP

# include "player.hpp"

class Server
{
	private:
		int		_port;
		Player	*_player_lst;

	public:
		Server();
		Server(const Server &src);
		Server &operator=(const Server &src);
		~Server();

		int	start_server(int port);

		int		getServerPort();
		void	setServerPort(int port);
}

#endif