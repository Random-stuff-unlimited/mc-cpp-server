#ifndef SERVER_HPP
# define SERVER_HPP

# include "player.hpp"
# include <mutex>
# include <netinet/in.h>
# include <unordered_map>
# include <string>
# include "enums.hpp"
# include "json.hpp"
# define ConfigFileName "config.json"

using json = nlohmann::json;

class Server
{
	private:
		std::unordered_map<int, Player*>	_playerLst;
		json								_playerSample;
		std::mutex							_playerLock;
		int									_protocolVersion;
		int									_serverSize;
		int									loadConfig();
		std::string							_gameVersion;
		std::string							_serverMOTD;
		int									_serverPort;
		char*								_serverAddr;

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
		std::unordered_map<int, Player*> &getPlayerLst() {return _playerLst;}

		void	addPlayerToSample(const std::string &name);
		void    removePlayerToSample(const std::string &name);
		Player 	*addPlayer(const std::string &name, const PlayerState state, const int socket);
		void 	removePlayer(Player *player);
		json	getPlayerSample();
};

#endif
