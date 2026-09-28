#ifndef PLAYER_HPP
#define PLAYER_HPP

#include "lib/UUID.hpp"
#include "world/item/PlayerInventory.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <mutex>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
class Server;
class ChunkStreamer;

enum class PlayerState { None, Configuration, Handshake, Status, Login, Play };

// Health and combat. Game thread only. Times are in ticks (TickLoop::getTickCount)
struct CombatState {
	float health	 = 20;
	int	  food		 = 20;
	float saturation = 5;
	bool  dead		 = false;

	int64_t invulnerableUntil = 0; // 10 ticks after a hit, only stronger hits get through
	float	lastDamage		  = 0;
	double	fallDistance	  = 0;
	int64_t lastRegeneration  = 0;

	bool	inCombat	= false;
	int64_t combatStart = 0;
	int64_t lastCombat	= 0;

	// Kill credit: the last player that hurt this one, for 100 ticks (death messages)
	std::string lastAttacker;
	int64_t		lastAttackedAt = 0;

	bool hasDeathLocation = false; // Where the player last died (recovery compass)
	int	 deathX = 0, deathY = 0, deathZ = 0;
};

// Encoded packets waiting to be written to the socket. Filled by any thread (Packet::send), emptied by the network
// thread that owns the connection
struct PlayerOutput {
	std::mutex			 mutex;
	std::vector<uint8_t> data;
	bool				 closing = false;	   // Disconnect requested: nothing more is queued, the socket closes once data is written
	std::atomic<bool>	 flushQueued{false}; // Already in its network thread's list of connections to write
};

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
	mutable std::mutex		 _nameMutex; // The name is set at login while other logins look for duplicates
	std::atomic<PlayerState> _state;
	int						 _socketFd;
	std::atomic<bool>		 _disconnected;
	int						 _networkThread = 0; // Index of the network thread that owns the socket
	PlayerOutput			 _output;
	std::atomic<int>		 _compressionThreshold{-1};
	std::atomic<int64_t>	 _keepAlivePending{0}; // Id of the unanswered Keep Alive, 0 if none
	std::atomic<int64_t>	 _keepAliveSentAt{0};  // Milliseconds, steady clock
	std::unique_ptr<ChunkStreamer> _chunkStreamer;

	// Game state. Only used on the game thread (see TickLoop)
	GameMode				_gameMode = GameMode::Survival;
	double					_posX = 0, _posY = 0, _posZ = 0;
	float					_yaw	  = 0; // Degrees, 0 = looking south (+z), 90 = west
	float					_pitch	  = 0; // Degrees, -90 = looking up
	bool					_onGround = false;
	PlayerInventory			_inventory;
	int						_selectedSlot = 3; // Hotbar index 0-8
	bool					_digging	  = false;
	bool					_sprinting	  = false;
	int64_t					_lastAttack	  = 0; // Tick of the last attack: the attack strength recharges from there
	CombatState				_combat;
	int						_digX = 0, _digY = 0, _digZ = 0;
	int						_blockChangesAck = -1; // Highest block action sequence to acknowledge, -1 if none
	int			  x, y, z;
	int			  health;
	UUID		  _uuid;
	int			  _playerId;
	Server&		  _server;
	PlayerConfig* _config;
	bool		  _shiftKeyDown = false;

  public:
	// The standing player's box (EntityDimensions 0.6 x 1.8, floats)
	static constexpr float BB_WIDTH = 0.6F, BB_HEIGHT = 1.8F;

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

	int			  getNetworkThread() const { return _networkThread; }
	void		  setNetworkThread(int index) { _networkThread = index; }
	PlayerOutput& output() { return _output; }

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
	// Sneak key held (Player Input packet): isSecondaryUseActive
	bool   isShiftKeyDown() const { return _shiftKeyDown; }
	void   setShiftKeyDown(bool down) { _shiftKeyDown = down; }
	float  getPitch() const { return _pitch; }
	bool   isOnGround() const { return _onGround; }
	void   setRotation(float yaw, float pitch) {
		  _yaw	 = yaw;
		  _pitch = pitch;
	}
	void   setOnGround(bool onGround) { _onGround = onGround; }

	PlayerInventory&		inventory() { return _inventory; }
	const PlayerInventory&	inventory() const { return _inventory; }
	int						getSelectedSlot() const { return _selectedSlot; }
	void					setSelectedSlot(int slot) { _selectedSlot = slot; }
	// Window slot of the hand: 0 = main hand (selected hotbar slot), 1 = offhand
	int						handSlot(int hand) const { return hand == 0 ? PlayerInventory::HOTBAR + _selectedSlot : PlayerInventory::OFFHAND; }
	const ItemStack&		getStackInHand(int hand) const { return _inventory.get(handSlot(hand)); }
	// Item ids, 0 (air) when empty
	int32_t getItemInHand(int hand) const { return getStackInHand(hand).isEmpty() ? 0 : getStackInHand(hand).item; }
	int32_t getInventoryItem(int slot) const {
		if (slot < 0 || slot >= PlayerInventory::SIZE || _inventory.get(slot).isEmpty()) return 0;
		return _inventory.get(slot).item;
	}

	CombatState& combat() { return _combat; }
	bool		 isSprinting() const { return _sprinting; }
	void		 setSprinting(bool sprinting) { _sprinting = sprinting; }
	int64_t		 getLastAttack() const { return _lastAttack; }
	void		 setLastAttack(int64_t time) { _lastAttack = time; }

	// Block being mined in survival (between the start and finish digging actions)
	void startDigging(int x, int y, int z) {
		_digging = true;
		_digX	 = x;
		_digY	 = y;
		_digZ	 = z;
	}
	void stopDigging() { _digging = false; }
	bool isDigging(int x, int y, int z) const { return _digging && _digX == x && _digY == y && _digZ == z; }

	// Block actions handled up to this sequence: acknowledged at the end of the tick, after the block changes
	void acknowledgeBlockChanges(int sequence) { _blockChangesAck = std::max(_blockChangesAck, sequence); }
	int	 takeBlockChangesAck() {
		 int sequence	  = _blockChangesAck;
		 _blockChangesAck = -1;
		 return sequence;
	}

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
	PlayerConfig*		getPlayerConfig() { return _config; }
	const PlayerConfig* getPlayerConfig() const { return _config; }
	int			  getPlayerID() const;
	void		  setUUID(UUID uuid);
	const UUID&	  getUUID() const { return _uuid; }
};

#endif
