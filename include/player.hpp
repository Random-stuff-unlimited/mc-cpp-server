#ifndef PLAYER_HPP
#define PLAYER_HPP

#include "lib/JavaRandom.hpp"
#include "lib/UUID.hpp"
#include "world/BlockPos.hpp"
#include "world/FoodData.hpp"
#include "world/entity/Actor.hpp"
#include "world/item/PlayerInventory.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <mutex>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
class Level;
class Menu;
class Server;
namespace nbt {
	struct TagCompound;
}
class ChunkStreamer;

enum class PlayerState { None, Configuration, Handshake, Status, Login, Play };

// Where the player respawns (vanilla's RespawnPosition): the bed or respawn anchor it slept in, with the dimension
// it is in. Only the dimension loaded by the server counts: a spawn elsewhere sends the player to the world spawn
struct PlayerSpawn {
	bool		valid = false;
	int			x = 0, y = 0, z = 0;
	std::string dimension; // "minecraft:overworld"
	bool		forced = false; // Set by /spawnpoint: respawn there even without a bed
};

// Health and combat. Game thread only. Times are in ticks (TickLoop::getTickCount)
struct CombatState {
	float health	 = 20;
	bool  dead		 = false;

	int64_t invulnerableUntil = 0; // 10 ticks after a hit, only stronger hits get through
	float	lastDamage		  = 0;
	double	fallDistance	  = 0;

	bool	inCombat	= false;
	int64_t combatStart = 0;
	int64_t lastCombat	= 0;

	// Kill credit: the last player that hurt this one, for 100 ticks (death messages)
	std::string lastAttacker;
	int64_t		lastAttackedAt = 0;

	bool		hasDeathLocation = false; // Where the player last died (recovery compass)
	int			deathX = 0, deathY = 0, deathZ = 0;
	std::string deathDimension;
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
// Vanilla's Pose, protocol ids (the player uses standing, crouching and swimming here)
enum class Pose : uint8_t { Standing = 0, FallFlying = 1, Sleeping = 2, Swimming = 3, SpinAttack = 4, Crouching = 5, LongJumping = 6, Dying = 7 };

// What Survival and ItemUse keep per player (vanilla's LivingEntity / Player / ServerPlayer fields). Game thread only
struct SurvivalState {
	int	 airSupply = 300; // Entity.getAirSupply, saved by vanilla as "Air"
	Pose pose	   = Pose::Standing;
	bool flying	   = false; // Abilities.flying
	// Entity.wasTouchingWater, isEyeInFluid(WATER) and isSwimming, as of the last tick
	bool	inWater = false, eyeInWater = false, swimming = false;
	int64_t tickCount = 0; // Entity.tickCount

	// LivingEntity.useItem and useItemRemaining, and DATA_LIVING_ENTITY_FLAGS (1: using an item, 2: with the offhand)
	ItemStack useItem;
	int		  useItemRemaining = 0;
	uint8_t	  livingFlags	   = 0;
	uint8_t	  dirtyData		   = 0; // Entity data changed since it was last sent (Survival::DATA_* bits)

	// ServerPlayer.lastSentHealth, lastSentFood and lastFoodSaturationZero: SET_HEALTH when they change
	float lastSentHealth		 = -1.0E8F;
	int	  lastSentFood			 = -99999999;
	bool  lastFoodSaturationZero = true;

	// Entity.remainingFireTicks: burning while > 0; -20 (Player.getFireImmuneTicks) once out of the fire
	int		remainingFireTicks = -20;
	uint8_t sharedFlags		   = 0; // Entity.DATA_SHARED_FLAGS_ID as last sent (on fire, crouching, sprinting...)

	// Sleeping in a bed (Player.SleepTimer / LivingEntity.sleeping): the pose is Sleeping, the timer counts to 100,
	// then the night is skipped (Server::tickSleeping). sleepingPos: the bed it sleeps in
	bool	 sleeping	= false;
	int	 sleepTimer	= 0;
	BlockPos sleepingPos;
};

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

class Player : public Actor, public std::enable_shared_from_this<Player> {
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
	Level*					_level	  = nullptr; // The dimension the player is in, once in game
	GameMode				_gameMode = GameMode::Survival;
	int						_previousGameMode = -1; // ServerPlayerGameMode.previousGameModeForPlayer, -1 = none
	// The experience bar (vanilla's XpBar): the total points, the level and the progress toward the next
	int		_xpTotal	= 0;
	int		_xpLevel	= 0;
	float	_xpProgress = 0.0F;
	int		_xpSeed		= 0; // XpSeed: the random for the enchanting table's offers
	Vec3					_pos;
	float					_yaw	  = 0; // Degrees, 0 = looking south (+z), 90 = west
	float					_pitch	  = 0; // Degrees, -90 = looking up
	bool					_onGround = false;
	PlayerInventory			_inventory;
	int						_selectedSlot = 3; // Hotbar index 0-8
	bool					_digging	  = false;
	bool					_sprinting	  = false;
	int64_t					_lastAttack	  = 0; // Tick of the last attack: the attack strength recharges from there
	CombatState				_combat;
	PlayerSpawn				_spawn; // The respawn point (bed or respawn anchor)
	int						_digX = 0, _digY = 0, _digZ = 0;
	int						_blockChangesAck = -1; // Highest block action sequence to acknowledge, -1 if none
	int			  x, y, z;
	int			  health;
	UUID		  _uuid;
	int			  _playerId;
	Server&		  _server;
	PlayerConfig* _config;
	bool		  _shiftKeyDown = false;
	std::array<ItemStack, 27> _enderChest;
	std::unique_ptr<Menu> _inventoryMenu, _openMenu;
	int					  _containerCounter = 0;
	bool				  _seenCredits		 = false;
	bool				  _wonGame			 = false;
	int					  _teleportId		 = 0; // Teleport id of the last Synchronize Player Position, echoed by Accept Teleportation
	std::array<bool, 8>	  _recipeBookSettings{}; // RecipeBookSettings: open and filtering, for crafting, furnace, blast furnace, smoker
	// The playerdata file this player was loaded from: written back with our values over it, so what the server
	// doesn't simulate yet (attributes, effects, advancements' data...) isn't lost. Null for a new player
	std::shared_ptr<const nbt::TagCompound> _savedData;
	FoodData			  _foodData;
	SurvivalState		  _survival;
	JavaRandom			  _random{0}; // Entity.random (seeded in the constructors)

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

	// The dimension the player is in (null before it enters the game). Changed by Server::changeDimension
	Level*	 level() const { return _level; }
	void	 setLevel(Level* level) { _level = level; }

	GameMode getGameMode() const { return _gameMode; }
	void	 setGameMode(GameMode mode) { _gameMode = mode; }
	// Protocol id of the game mode before the last change, -1 if none
	int		 getPreviousGameMode() const { return _previousGameMode; }
	void	 setPreviousGameMode(int mode) { _previousGameMode = mode; }

	// The experience bar (XpBar): the total points, the level and the progress toward the next
	int	 getXpTotal() const { return _xpTotal; }
	void setXpTotal(int total) { _xpTotal = total; }
	int	 getXpLevel() const { return _xpLevel; }
	void setXpLevel(int level) { _xpLevel = level; }
	float getXpProgress() const { return _xpProgress; }
	void  setXpProgress(float progress) { _xpProgress = progress; }
	int	 getXpSeed() const { return _xpSeed; }
	void setXpSeed(int seed) { _xpSeed = seed; }

	void   setPosition(double x, double y, double z) { _pos = {x, y, z}; }
	double getX() const { return _pos.x; }
	double getY() const { return _pos.y; }
	double getZ() const { return _pos.z; }

	// ----- Actor -----
	int			id() const override { return _playerId; }
	const UUID& uuid() const override { return _uuid; }
	int			typeId() const override;
	Level*		actorLevel() const override { return _level; }
	const Vec3& position() const override { return _pos; }
	// Its box for its current pose (standing, crouching, swimming, sleeping)
	AABB		boundingBox() const override;
	double		eyeY() const override;
	bool		isAlive() const override { return !_combat.dead && !isDisconnected(); }
	bool		isSpectator() const override { return _gameMode == GameMode::Spectator; }
	bool		isCreative() const { return _gameMode == GameMode::Creative; }
	// Combat::damage
	bool		hurtServer(const Combat::DamageSource& source, float amount) override;
	// The client moves the player: it is told its new movement (SET_ENTITY_MOTION, to it and its viewers)
	void		pushMotion(const Vec3& impulse) override;
	void		igniteForTicks(int ticks) override;
	float		yRot() const override { return _yaw; }
	Player*		asPlayer() override { return this; }
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
	// The ender chest's 27 slots (PlayerEnderChestContainer)
	std::array<ItemStack, 27>& enderChest() { return _enderChest; }
	// Menus (game thread): the inventory's own, always there once made, and the one open over it (see Menus)
	std::unique_ptr<Menu>&	inventoryMenuSlot() { return _inventoryMenu; }
	std::unique_ptr<Menu>&	openMenuSlot() { return _openMenu; }
	// ServerPlayer.nextContainerCounter: 1 to 100
	int						nextContainerCounter() { return _containerCounter = _containerCounter % 100 + 1; }
	int	 portalCooldown() const { return portal.cooldown; }
	// ServerPlayer.seenCredits / wonGame: the end credits were shown once; they are showing now (the player left its
	// level and respawns in the overworld when the client closes them)
	bool seenCredits() const { return _seenCredits; }
	void setSeenCredits(bool seen) { _seenCredits = seen; }
	bool wonGame() const { return _wonGame; }
	void setWonGame(bool won) { _wonGame = won; }
	int	 dimensionChangingDelay() const override { return 10; }
	// A new teleport id for the next Synchronize Player Position
	int						nextTeleportId() { return ++_teleportId; }
	std::array<bool, 8>&	recipeBookSettings() { return _recipeBookSettings; }
	const std::array<bool, 8>& recipeBookSettings() const { return _recipeBookSettings; }
	const std::array<ItemStack, 27>& enderChest() const { return _enderChest; }
	const std::shared_ptr<const nbt::TagCompound>& savedData() const { return _savedData; }
	void setSavedData(std::shared_ptr<const nbt::TagCompound> data) { _savedData = std::move(data); }
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
	const CombatState& combat() const { return _combat; }
	// The respawn point: a bed or a respawn anchor, set when the player sleeps in one (or by /spawnpoint)
	PlayerSpawn&		spawn() { return _spawn; }
	const PlayerSpawn&	spawn() const { return _spawn; }
	// Hunger (Player.getFoodData): food level, saturation, exhaustion and tick timer, with their getters and setters
	FoodData&		foodData() { return _foodData; }
	const FoodData& foodData() const { return _foodData; }
	// Air (Entity.getAirSupply / setAirSupply, 300 = full): sent to the client in the entity data
	int	 getAirSupply() const { return _survival.airSupply; }
	void setAirSupply(int air) {
		if (air != _survival.airSupply) _survival.dirtyData |= 1;
		_survival.airSupply = air;
	}
	SurvivalState&		 survival() { return _survival; }
	const SurvivalState& survival() const { return _survival; }
	JavaRandom&			 random() { return _random; }
	// LivingEntity.isUsingItem / getUsedItemHand (0 main hand, 1 offhand)
	bool isUsingItem() const { return _survival.livingFlags & 1; }
	int	 getUsedItemHand() const { return _survival.livingFlags & 2 ? 1 : 0; }
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
