#ifndef PLAYER_HPP
#define PLAYER_HPP

#include "UUID.hpp"

#include <string>
<<<<<<< HEAD
#include <cstdint>
=======
class Server;
>>>>>>> refs/remotes/origin/main

enum class PlayerState { None, Configuration ,Handshake, Status, Login, Play };

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

	// Player Configuration
	int _chatMode;
	int _mainHand;
	std::string _locale;
	uint8_t _viewDistance;
	uint8_t _displayedSkinParts;
	bool _chatColors;
	bool _enableTextFiltering;
	bool _allowServerListings;

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
<<<<<<< HEAD

	// Getters Player Configuration
	int getChatMode() { return _chatMode; }
	std::string getLocale() { return _locale; }
	uint8_t getViewDistance() { return _viewDistance; }
	uint8_t getDisplayedSkinParts() { return _displayedSkinParts; }
	bool getChatColorsStatus() { return _chatColors; }
	bool getTextFilteringStatus() { return _enableTextFiltering; }
	bool getServerListingsStatus() { return _allowServerListings; }
	int getMainHand() { return _mainHand; }

	// Setters Player Configuration
	void setChatMode(int mode) {_chatMode = mode;}
	void setMainHand(int mainHand) {_mainHand = mainHand;}
	void setLocale(std::string locale) {_locale = locale;}
	void setViewDistance(uint8_t viewDistance) {_viewDistance = viewDistance;}
	void setDisplayedSkinParts(uint8_t skinParts) {_displayedSkinParts = skinParts;}
	void setChatColors(bool chatColors) {_chatColors = chatColors;}
	void setEnabledTextFiltering(bool textFiltering) {_enableTextFiltering = textFiltering;}
	void setAllowServerListings(bool serverListings) {_allowServerListings = serverListings;}
=======
	int getPlayerID() const;
	void setUUID(UUID uuid);
>>>>>>> refs/remotes/origin/main
};

#endif
