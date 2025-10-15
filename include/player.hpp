#ifndef PLAYER_HPP
#define PLAYER_HPP

#include "lib/UUID.hpp"

#include <cstdint>
#include <string>
class Server;

enum class PlayerState { None, Configuration, Handshake, Status, Login, Play };

class PlayerConfig {
  private:
	int			_chatMode;
	int			_mainHand;
	std::string _locale;
	uint8_t		_viewDistance;
	uint8_t		_displayedSkinParts;
	bool		_chatColors;
	bool		_enableTextFiltering;
	bool		_allowServerListings;

  public:
	PlayerConfig();
	~PlayerConfig();

	// Getters
	int			getChatMode() const { return _chatMode; }
	int			getMainHand() const { return _mainHand; }
	std::string getLocale() const { return _locale; }
	uint8_t		getViewDistance() const { return _viewDistance; }
	uint8_t		getDisplayedSkinParts() const { return _displayedSkinParts; }
	bool		getChatColors() const { return _chatColors; }
	bool		getTextFiltering() const { return _enableTextFiltering; }
	bool		getServerListings() const { return _allowServerListings; }

	// Setters
	void setChatMode(int mode) { _chatMode = mode; }
	void setMainHand(int mainHand) { _mainHand = mainHand; }
	void setLocale(std::string locale) { _locale = locale; }
	void setViewDistance(uint8_t viewDistance) { _viewDistance = viewDistance; }
	void setDisplayedSkinParts(uint8_t skinParts) { _displayedSkinParts = skinParts; }
	void setChatColors(bool chatColors) { _chatColors = chatColors; }
	void setTextFiltering(bool textFiltering) { _enableTextFiltering = textFiltering; }
	void setServerListings(bool serverListings) { _allowServerListings = serverListings; }
};

struct vec3 {
	double x, y, z;
};

class PlayerEntity {
  private:
	vec3 _position;
};

class Player {
  private:
	std::string	  _name;
	PlayerState	  _state;
	int			  _socketFd;
	int			  x, y, z;
	int			  health;
	UUID		  _uuid;
	int			  _playerId;
	Server&		  _server;
	PlayerConfig* _config;

  public:
	Player(Server& server);
	Player(const std::string& name, PlayerState state, int socket, Server& server);
	Player& operator=(const Player& src);
	~Player();

	std::string getPlayerName(void);
	void		setPlayerName(const std::string& name);
	PlayerState getPlayerState();
	void		setPlayerState(PlayerState state);
	void		setSocketFd(int socket);
	int			getSocketFd() const;

	// Get PlayerConfig instance
	PlayerConfig* getPlayerConfig() { return _config; }
	int			  getPlayerID() const;
	void		  setUUID(UUID uuid);
};

#endif
