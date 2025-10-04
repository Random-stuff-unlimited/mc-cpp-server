#ifndef PLAYER_HPP
#define PLAYER_HPP

#include "UUID.hpp"

#include <string>
class Server;

enum class PlayerState { None, Handshake, Status, Login, Play };

class Player {
  private:
	std::string _name;
	PlayerState _state;
	int _socketFd;
	int x, y, z;
	int health;
	UUID _uuid;
	int _playerId;
	Server& _server;

  public:
	Player(Server& server);
	Player(const std::string& name, PlayerState state, int socket, Server& server);
	Player& operator=(const Player& src);
	~Player();

	std::string getPlayerName(void);
	void setPlayerName(const std::string& name);
	PlayerState getPlayerState();
	void setPlayerState(PlayerState state);
	void setSocketFd(int socket);
	int getSocketFd() const;
	int getPlayerID() const;
	void setUUID(UUID uuid);
};

#endif
