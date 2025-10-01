#ifndef SERVER_HPP
# define SERVER_HPP

# include "player.hpp"
# include <mutex>
# include <netinet/in.h>
# define ConfigFileName "config.json"

class Server
{
	private:
		Player				*_player_lst;		
		std::mutex			_playerLock; 
		int					_protocolVersion;
		int					_serverSize;
		int					loadConfig();
		std::string			_gameVersion;
		std::string 		_serverMOTD;
		
	public:
		Server();
		~Server();

		int	start_server(int port);

		int			getProtocolVersion();
		int			getServerSize();
		int			getAmountOnline();
		std::string	getGameVersion();
		std::string getServerMOTD();

};

#endif