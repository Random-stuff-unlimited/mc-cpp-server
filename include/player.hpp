#ifndef PLAYER_HPP
#define PLAYER_HPP

#include "UUID.hpp"
#include "enums.hpp"

#include <string>

class Player {
  private:
	std::string _name;
	PlayerState _state;
	int _socketFd;
	int x, y, z;
	int health;
	UUID _uuid;

  public:
	Player();
	Player(const std::string& name, PlayerState state, int socket);
	Player& operator=(const Player& src);
	~Player();

	std::string getPlayerName(void);
	void setPlayerName(const std::string& name);
	PlayerState getPlayerState();
	void setPlayerState(PlayerState state);
	void setSocketFd(int socket);
	int getSocketFd() const;
};

#endif
