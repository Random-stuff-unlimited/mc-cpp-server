#ifndef SERVER_HPP
# define SERVER_HPP

# include "player.hpp"
# include <mutex>
# include <netinet/in.h>
# include <vector>
#include "json.hpp"
# define ConfigFileName "config.json"

using json = nlohmann::json;

class Server
{
	private:
		std::vector<Player>	_playerLst;
		json				_playerSample;
		std::mutex			_playerLock;
		int					_protocolVersion;
		int					_serverSize;
		int					loadConfig();
		std::string			_gameVersion;
		std::string 		_serverMOTD;
		int 				_serverPort;
		char*				_serverAddr;
		
	public:
		Server();
		~Server();

		int	start_server(int port);

		int			getProtocolVersion();
		int			getServerSize();
		int			getAmountOnline();
		std::string	getGameVersion();
		std::string getServerMOTD();
		int			getServerPort() {return _serverPort;}
		char*		getServerAddr() {return _serverAddr;}

		void	addPlayerToSample(const std::string &name);
		void    removePlayerToSample(const std::string &name);
		void 	addPlayer(Player *player);
		json	getPlayerSample();
};

#endif