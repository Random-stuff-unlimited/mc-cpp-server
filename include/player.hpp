#ifndef PLAYER_HPP
#define PLAYER_HPP

#include "lib/UUID.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
class Server;
class ChunkStreamer;

enum class PlayerState { None, Configuration, Handshake, Status, Login, Play };

// Protocol ids
enum class GameMode : uint8_t { Survival = 0, Creative = 1, Adventure = 2, Spectator = 3 };

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

class Player : public std::enable_shared_from_this<Player> {
  private:
	std::string				 _name;
	std::atomic<PlayerState> _state;
	int						 _socketFd;
	std::atomic<bool>		 _disconnected;
	bool					 _socketClosed; // Only touched by the sender thread
	std::atomic<int>		 _compressionThreshold{-1};
	std::atomic<int64_t>	 _keepAlivePending{0}; // Id of the unanswered Keep Alive, 0 if none
	std::atomic<int64_t>	 _keepAliveSentAt{0};  // Milliseconds, steady clock
	std::unique_ptr<ChunkStreamer> _chunkStreamer;

	// Game state. Only touched by the worker thread handling this player's packets
	GameMode				_gameMode = GameMode::Survival;
	double					_posX = 0, _posY = 0, _posZ = 0;
	float					_yaw  = 0; // Degrees, 0 = looking south (+z), 90 = west
	std::array<int32_t, 46> _inventory;		  // Item id per inventory slot (window numbering: hotbar is 36-44, offhand 45), -1 = empty
	int						_selectedSlot = 3; // Hotbar index 0-8
	bool					_digging	  = false;
	int						_digX = 0, _digY = 0, _digZ = 0;
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

	// Returns true only for the first caller, so teardown runs exactly once
	bool markDisconnected() { return !_disconnected.exchange(true); }
	bool isDisconnected() const { return _disconnected.load(); }
	bool isSocketClosed() const { return _socketClosed; }
	void setSocketClosed() { _socketClosed = true; }

	// -1 until Set Compression is sent, then packets of at least this size are compressed (both directions)
	int	 getCompressionThreshold() const { return _compressionThreshold.load(); }
	void setCompressionThreshold(int threshold) { _compressionThreshold.store(threshold); }

	GameMode getGameMode() const { return _gameMode; }
	void	 setGameMode(GameMode mode) { _gameMode = mode; }

	void   setPosition(double x, double y, double z) {
		  _posX = x;
		  _posY = y;
		  _posZ = z;
	}
	double getX() const { return _posX; }
	double getY() const { return _posY; }
	double getZ() const { return _posZ; }
	float  getYaw() const { return _yaw; }
	void   setYaw(float yaw) { _yaw = yaw; }

	void	setInventorySlot(int slot, int32_t item) {
		   if (slot >= 0 && slot < static_cast<int>(_inventory.size())) _inventory[slot] = item;
	}
	int		getSelectedSlot() const { return _selectedSlot; }
	void	setSelectedSlot(int slot) { _selectedSlot = slot; }
	int32_t getItemInHand(int hand) const { return hand == 0 ? _inventory[36 + _selectedSlot] : _inventory[45]; }

	// Block being mined in survival (between the start and finish digging actions)
	void startDigging(int x, int y, int z) {
		_digging = true;
		_digX	 = x;
		_digY	 = y;
		_digZ	 = z;
	}
	void stopDigging() { _digging = false; }
	bool isDigging(int x, int y, int z) const { return _digging && _digX == x && _digY == y && _digZ == z; }

	// Created when the player enters the Play state
	void		   createChunkStreamer();
	ChunkStreamer* getChunkStreamer() { return _chunkStreamer.get(); }

	int64_t getKeepAlivePending() const { return _keepAlivePending.load(); }
	int64_t getKeepAliveSentAt() const { return _keepAliveSentAt.load(); }
	void	setKeepAliveSent(int64_t id, int64_t sentAt) {
		   _keepAliveSentAt.store(sentAt);
		   _keepAlivePending.store(id);
	}
	void onKeepAliveResponse(int64_t id) { _keepAlivePending.compare_exchange_strong(id, 0); }

	// Get PlayerConfig instance
	PlayerConfig* getPlayerConfig() { return _config; }
	int			  getPlayerID() const;
	void		  setUUID(UUID uuid);
};

#endif
