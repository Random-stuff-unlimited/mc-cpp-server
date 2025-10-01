#ifndef SERVER_HPP
# define SERVER_HPP

# include "player.hpp"
# define ConfigFileName "config.json"

class Server
{
	private:
		int			_port;
		int			_protocolVersion;
		int			_serverSize;
		int			loadConfig();
		Player		*_player_lst;
		std::string	_gameVersion;
		std::string _serverMOTD;
		
	public:
		Server();
		Server(const Server &src);
		Server &operator=(const Server &src);
		~Server();

		int	start_server(int port);

		int			getServerPort();
		int			getProtocolVersion();
		int			getServerSize();
		int			getAmountOnline();
		void		setServerPort(int port);
		std::string	getGameVersion();
		std::string getServerMOTD();

};

#endif