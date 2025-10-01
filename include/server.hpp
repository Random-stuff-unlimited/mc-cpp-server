#ifndef SERVER_HPP
# define SERVER_HPP

# include "player.hpp"
# include <netinet/in.h>

class Server
{
	private:
		Player				*_player_lst;
		std::atomic<bool>	_stopServer;

	public:
		Server();
		Server(const Server &src);
		Server &operator=(const Server &src);
		~Server();

		int	start_server(int port);

		int		getServerPort();
		void	setServerPort(int port);
		int		getServerAddr();
		void	setServerAddr(int addr);

		int	init_thread();
}

#endif