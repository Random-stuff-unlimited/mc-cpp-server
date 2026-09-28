#ifndef LEVEL_HPP
#define LEVEL_HPP

#include "data/BlockRegistry.hpp"
#include "lib/JavaRandom.hpp"
#include "world/BlockBehavior.hpp"
#include "world/Fluids.hpp"
#include "world/BlockPos.hpp"
#include "world/LevelTicks.hpp"
#include "world/NeighborUpdater.hpp"
#include "world/entity/EntityManager.hpp"
#include "world/entity/MobRegistry.hpp"
#include "world/entity/Geometry.hpp"
#include "world/item/LootTables.hpp"
#include "world/item/Recipes.hpp"
#include "world/World.hpp"
#include "world/blockentity/BlockEntity.hpp"

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Chunk;
class FuelValues;
class GameData;
class PotionBrewing;
class Player;
class Server;

// The world as the game logic sees it, on the game thread only: vanilla's Level / ServerLevel.
//
// Blocks are read without any lock (the game thread is the only one changing them; it locks a chunk only while
// writing, for the I/O threads that save and encode it). Every change goes through setBlock, which runs the same
// steps as vanilla in the same order: onPlace / affectNeighborsAfterRemoval, neighbor updates, shape updates.
// Scheduled ticks and block events run in the tick phases called by Server::tick, in vanilla's order.
// Changes reach the players once per tick (sendChanges): light first, then one packet per changed section.
class Level : public NeighborUpdateTarget {
  public:
	// Block.UPDATE_* flags of setBlock
	static constexpr int UPDATE_NEIGHBORS					 = 1;
	static constexpr int UPDATE_CLIENTS						 = 2;
	static constexpr int UPDATE_INVISIBLE					 = 4;
	static constexpr int UPDATE_IMMEDIATE					 = 8;
	static constexpr int UPDATE_KNOWN_SHAPE					 = 16;
	static constexpr int UPDATE_SUPPRESS_DROPS				 = 32;
	static constexpr int UPDATE_MOVE_BY_PISTON				 = 64;
	static constexpr int UPDATE_SKIP_SHAPE_UPDATE_ON_WIRE	 = 128;
	static constexpr int UPDATE_SKIP_BLOCK_ENTITY_SIDEEFFECTS = 256;
	static constexpr int UPDATE_SKIP_ON_PLACE				 = 512;
	static constexpr int UPDATE_ALL							 = 3;
	static constexpr int UPDATE_ALL_IMMEDIATE				 = 11;
	static constexpr int UPDATE_LIMIT						 = 512; // Depth of chained shape updates
	static constexpr int MAX_TICKS_PER_TICK					 = 65536;

	Level(Server& server, World& world, const GameData& gameData);

	const BlockRegistry& blocks() const { return _blocks; }
	BlockBehaviors&		 behaviors() { return _behaviors; }
	// gamedata/recipes.json, then the recipes/ folder beside gamedata/
	const RecipeManager& recipes() const { return _recipes; }
	const BlockBehavior& behavior(int state) const { return _behaviors.get(_blocks.blockOf(state)); }
	int64_t				 getGameTime() const { return _world.getGameTime(); }
	// The dimension this level is: the world's name ("minecraft:overworld")
	const std::string& dimensionName() const { return _world.getDimensionName(); }
	Server&				 server() { return _server; }
	const GameData&		 gameData() const { return _gameData; }
	EntityManager&		 entities() { return _entities; }
	// How each mob type is made and which goals it gets (the AI's registration point)
	MobRegistry&		 mobs() { return _mobs; }
	// Block and entity loot tables
	const LootTables&	 loot() const { return _loot; }
	// Level.addFreshEntity: the entity joins the level and the players around see it
	Entity*				 addFreshEntity(std::unique_ptr<Entity> entity) { return _entities.add(std::move(entity)); }
	int					 minY() const { return _minY; }
	int					 maxY() const { return _maxY; } // Exclusive
	// Nether-like dimension: lava flows faster and further
	bool isUltraWarm() const { return _ultraWarm; }
	// Block ids entities check
	int slimeBlock() const { return _slimeBlock; }
	int waterBlock() const { return _waterBlockId; }
	int bubbleColumnBlock() const { return _bubbleColumnBlock; }
	int fireBlock() const { return _fire; }
	int soulFireBlock() const { return _soulFire; }
	int cactusBlock() const { return _cactusBlock; }
	Fluids&				 fluids() { return *_fluids; }
	// The world's random source (vanilla's level random)
	JavaRandom&			 random() { return _random; }
	// ----- Furnaces and brewing stands -----
	// Level.fuelValues and Level.potionBrewing: the vanilla fuel table and brewing mixes (made on first use)
	const FuelValues&	 fuelValues();
	const PotionBrewing& potionBrewing();
	// ----- End furnaces and brewing stands -----

	// ----- Reading -----

	// void_air outside the world or in a chunk that isn't loaded
	int		   getBlockState(const BlockPos& pos) override;
	FluidState getFluidState(const BlockPos& pos) { return _fluids->stateOf(getBlockState(pos)); }
	// Light level (Level.getRawBrightness): the sky light minus skyDarken, or the block light if higher. 0 outside
	// loaded chunks, 15 above the world
	int	 getRawBrightness(const BlockPos& pos, int skyDarken = 0);
	// Block light alone (getBrightness(LightLayer.BLOCK))
	int	 getBlockLight(const BlockPos& pos) { return lightAt(pos, false); }
	// Full sky light here (LevelReader.canSeeSky)
	bool canSeeSky(const BlockPos& pos) { return lightAt(pos, true) >= 15; }
	bool hasChunkAt(const BlockPos& pos) { return chunkAt(pos.chunkX(), pos.chunkZ()) != nullptr; }
	// The chunk at these chunk coordinates if it is loaded, nullptr otherwise
	Chunk* loadedChunk(int chunkX, int chunkZ) { return chunkAt(chunkX, chunkZ); }
	bool isOutsideBuildHeight(int y) const { return y < _minY || y >= _maxY; }
	// Whether blocks tick at this position (vanilla's shouldTickBlocksAt)
	bool shouldTickBlocksAt(const BlockPos& pos);
	// How much the night takes from the sky light (Level.skyDarken, 0 at noon, 11 at midnight)
	int	 skyDarken() const { return _skyDarken; }
	// LevelReader.getMaxLocalRawBrightness: the light with the sky darkened by the time of day
	int	 getMaxLocalRawBrightness(const BlockPos& pos) { return getRawBrightness(pos, _skyDarken); }
	// LightEngine.getLightBlockInto: light absorbed going from `from` into `to` (its neighbor in `direction`), 16 if
	// their shapes close the face between them
	int	 getLightBlockInto(int from, int to, Direction direction, int lightBlock) const;
	// Level.getBlockRandomPos: a position in the 16x16 column at x, z, from y (vanilla's own LCG)
	BlockPos getBlockRandomPos(int x, int y, int z, int yMask);
	// The randomTickSpeed game rule
	int	 randomTickSpeed() const { return _randomTickSpeed; }
	void setRandomTickSpeed(int speed) { _randomTickSpeed = speed; }

	// ----- Writing (Level.setBlock and friends) -----

	// Returns false if nothing changed (same state, unloaded chunk, outside the world)
	bool setBlock(const BlockPos& pos, int state, int flags, int limit = UPDATE_LIMIT);
	// Level.sendBlockUpdated: the players get the block at pos again (after a change without UPDATE_CLIENTS)
	void sendBlockUpdated(const BlockPos& pos) { markChanged(pos); }
	// Replaces the block by its fluid (water if waterlogged) or air
	bool removeBlock(const BlockPos& pos, bool movedByPiston);
	// Breaks the block (Level.destroyBlock): break effect for the players, drops if `drop`
	bool destroyBlock(const BlockPos& pos, bool drop, int limit = UPDATE_LIMIT);
	// Block that stays when this one is removed: its fluid (vanilla FluidState.createLegacyBlock) or air
	int fluidLegacyBlock(int state) const { return _fluids->legacyBlock(_fluids->stateOf(state)); }
	// Block.dropResources: the block's loot as items around pos. breaker/tool: the player breaking it and its tool. The
	// block's entity (for its components) is the one still there, or the one just removed from pos this tick
	void dropResources(int state, const BlockPos& pos, Player* breaker = nullptr, const ItemStack* tool = nullptr);
	// Block.popResource: an item at a random point of the block
	void popResource(const BlockPos& pos, ItemStack stack);
	// LivingEntity.drop: an item thrown from the player's eyes, forward
	void dropFromPlayer(Player& player, ItemStack stack, bool traceable);
	// Entity phase of the tick (items...), and their changes to the players at the end of it
	void tickEntities() { _entities.tick(); }
	void sendEntityChanges() { _entities.sendChanges(); }

	// ----- Updates -----

	void updateNeighborsAt(const BlockPos& pos, int sourceBlock);
	void updateNeighborsAtExceptFromFacing(const BlockPos& pos, int sourceBlock, Direction skip);
	void neighborChanged(const BlockPos& pos, int sourceBlock);
	void neighborChanged(int state, const BlockPos& pos, int sourceBlock, bool movedByPiston);
	void neighborShapeChanged(Direction direction, const BlockPos& pos, const BlockPos& neighborPos, int neighborState, int flags, int limit);
	// The six neighbors of pos adapt to `state` (BlockState.updateNeighbourShapes)
	void updateNeighbourShapes(const BlockPos& pos, int state, int flags, int limit = UPDATE_LIMIT);
	// Comparators around: the analog output of pos changed (Level.updateNeighbourForOutputSignal)
	void updateNeighbourForOutputSignal(const BlockPos& pos, int sourceBlock);

	// ----- Redstone (vanilla's SignalGetter) -----

	// BlockState.isRedstoneConductor: power goes through it
	bool isRedstoneConductor(int state) const;
	bool isSignalSource(int state) const { return behavior(state).isSignalSource(state); }
	int	 getDirectSignal(const BlockPos& pos, Direction direction);
	// Strong power into pos from its six neighbors
	int	 getDirectSignalTo(const BlockPos& pos);
	// What a diode reads on its side (repeaters: other diodes only)
	int	 getControlInputSignal(const BlockPos& pos, Direction direction, bool diodesOnly);
	// Power from the block at pos toward direction's opposite side (conductors: what powers them)
	int	 getSignal(const BlockPos& pos, Direction direction);
	bool hasSignal(const BlockPos& pos, Direction direction) { return getSignal(pos, direction) > 0; }
	bool hasNeighborSignal(const BlockPos& pos);
	int	 getBestNeighborSignal(const BlockPos& pos);
	// ServerLevel.isHandlingTick: from the start of the tick to the block events
	bool isHandlingTick() const { return _handlingTick; }
	void setHandlingTick(bool handling) { _handlingTick = handling; }
	// RedstoneTorchBlock.RECENT_TOGGLES of this level: position and game time
	std::vector<std::pair<BlockPos, int64_t>>& recentTorchToggles() { return _torchToggles; }

	// ----- Block entities -----

	// The block entity at pos, nullptr if none (or its chunk isn't loaded)
	BlockEntity* getBlockEntity(const BlockPos& pos);
	// The same, shared: kept alive by whoever holds it (an open menu) even after it leaves the level
	std::shared_ptr<BlockEntity> getSharedBlockEntity(const BlockPos& pos);
	template <typename T> T* getBlockEntity(const BlockPos& pos) { return dynamic_cast<T*>(getBlockEntity(pos)); }
	// LevelChunk.setBlockEntity: replaces the one at its position (which keeps its place among the tickers)
	void		 setBlockEntity(std::unique_ptr<BlockEntity> entity);
	// Level.removeBlockEntity. The object stays valid until the end of the tick phase
	void		 removeBlockEntity(const BlockPos& pos);
	// Level.blockEntityChanged: its chunk must be saved; comparators read it again
	void		 blockEntityChanged(const BlockPos& pos);
	// ComparatorBlockEntity.getOutputSignal: 0 without one
	int			 comparatorOutput(const BlockPos& pos);
	void		 setComparatorOutput(const BlockPos& pos, int output);
	// Block entity phase of the tick (Level.tickBlockEntities), after the entities
	void		 tickBlockEntities();
	// PistonMovingBlockEntity.tick and finalTick, given by the pistons' behavior (they need its block ids)
	void setMovingPistonTicker(std::function<void(PistonMovingBlockEntity&)> tick, std::function<void(PistonMovingBlockEntity&)> finalTick) {
		_tickMovingPiston = std::move(tick);
		_finalTickPiston  = std::move(finalTick);
	}
	void tickMovingPiston(PistonMovingBlockEntity& piston) {
		if (_tickMovingPiston) _tickMovingPiston(piston);
	}
	void finalTickMovingPiston(PistonMovingBlockEntity& piston) {
		if (_finalTickPiston) _finalTickPiston(piston);
	}

	// ----- Sounds and entities -----

	enum class SoundSource { Master, Music, Records, Weather, Blocks, Hostile, Neutral, Players, Ambient, Voice, Ui };
	// Level.playSound: a sound at the center of pos for the players in range, except `except` (who played it already)
	void playSound(Player* except, const BlockPos& pos, const std::string& sound, SoundSource source, float volume = 1.0F, float pitch = 1.0F) {
		playSoundAt(except, pos.x + 0.5, pos.y + 0.5, pos.z + 0.5, sound, source, volume, pitch);
	}
	void playSoundAt(Player* except, double x, double y, double z, const std::string& sound, SoundSource source, float volume = 1.0F,
					 float pitch = 1.0F);
	// Entities touching the box (EntitySelector.NO_SPECTATORS): players and the other entities, or living ones only
	int	 countEntities(const AABB& box, bool livingOnly);
	// Entity.checkInsideBlocks: the blocks whose cell the box touches learn it (pressure plates)
	void checkInsideBlocks(const AABB& box, Entity* entity);
	// Level.noCollision (blocks only): whether a block's collision shape overlaps the box
	bool hasBlockCollision(const AABB& box);

	// ----- Scheduled ticks and block events -----

	void scheduleTick(const BlockPos& pos, int block, int delay, int priority = NORMAL);
	void scheduleFluidTick(const BlockPos& pos, int fluid, int delay, int priority = NORMAL);
	// What SimpleWaterloggedBlocks do in updateShape: if waterlogged, its water flows again
	void scheduleWaterlogged(const BlockPos& pos, int state);
	bool hasScheduledTick(const BlockPos& pos, int block) const { return _blockTicks.hasScheduledTick(pos, block); }
	bool willTickThisTick(const BlockPos& pos, int block) { return _blockTicks.willTickThisTick(pos, block); }
	// Run at the end of the tick (pistons, note blocks...), each distinct event once
	void blockEvent(const BlockPos& pos, int block, int type, int data);
	// Particles and sounds (Level Event packet) for the players within 64 blocks, except `except`
	void levelEvent(Player* except, int type, const BlockPos& pos, int data);

	// ----- Tick phases, in vanilla's order (see Server::tick) -----

	// Level.updateSkyBrightness: before the time moves
	void updateSkyBrightness();
	void tickScheduled(); // Block ticks, then fluid ticks
	// Random ticks of the ticking chunks (ServerChunkCache.tickChunks / ServerLevel.tickChunk)
	void tickChunks();
	void runBlockEvents();
	// Light and block changes of this tick to the players that have the chunks
	void sendChanges();
	// Forgets the chunks the world unloaded. About once per second
	void dropUnloadedChunks();
	// The entities to save go into their chunks (marked modified when they changed): before the world saves or
	// unloads chunks (World::tick, World::shutdown)
	void saveEntities();
	// Game thread: a chunk finished loading (its saved ticks start counting)
	void onChunkLoaded(const std::shared_ptr<Chunk>& chunk);

	// ----- NeighborUpdateTarget -----

	void executeNeighborChanged(int state, const BlockPos& pos, int sourceBlock, bool movedByPiston) override;
	void executeShapeUpdate(Direction direction, const BlockPos& pos, const BlockPos& neighborPos, int neighborState, int flags, int limit) override;

  private:
	struct BlockEvent {
		BlockPos pos;
		int		 block, type, data;
		bool	 operator==(const BlockEvent& other) const {
			return pos == other.pos && block == other.block && type == other.type && data == other.data;
		}
	};
	struct BlockEventHash {
		size_t operator()(const BlockEvent& event) const noexcept {
			return std::hash<int64_t>()(event.pos.asLong()) ^ (static_cast<size_t>(event.block) << 20) ^ (static_cast<size_t>(event.type) << 8) ^
				   static_cast<size_t>(event.data);
		}
	};

	Server&				 _server;
	World&				 _world;
	const GameData&		 _gameData;
	const BlockRegistry& _blocks;
	BlockBehaviors		 _behaviors;
	RecipeManager		 _recipes;
	NeighborUpdater		 _neighborUpdater;
	LevelTicks			 _blockTicks;
	LevelTicks			 _fluidTicks;
	int64_t				 _subTickCount = 0;
	int					 _minY, _maxY;

	// Loaded chunks as seen by the game thread, with the last one used (most reads are near each other)
	std::unordered_map<int64_t, std::shared_ptr<Chunk>> _chunks;
	int64_t												_lastKey   = 0;
	Chunk*												_lastChunk = nullptr;

	std::deque<BlockEvent>								_blockEvents;
	std::unordered_set<BlockEvent, BlockEventHash>		_blockEventSet;

	std::unordered_map<int64_t, std::vector<uint16_t>> _changedSections; // Section key -> changed positions in it
	std::unordered_set<int64_t>							_lightChecks;	  // Positions whose light may have changed

	// Ids used by the core
	int				   _voidAir, _air, _comparator, _redstoneWire, _fire, _soulFire, _waterlogged;
	std::vector<bool>  _simpleWaterlogged; // Per block: SimpleWaterloggedBlock, its water flows when waterlogged
	std::unique_ptr<Fluids> _fluids;
	JavaRandom				_random;
	bool					_ultraWarm;
	int						_slimeBlock, _waterBlockId, _bubbleColumnBlock, _cactusBlock;
	int						_skyDarken = 0;
	int						_randValue;		// getBlockRandomPos's LCG
	int						_randomTickSpeed = 3;
	std::vector<uint8_t>	_randomTicks;	// Per state: RANDOM_BLOCK and RANDOM_FLUID
	bool					_handlingTick = false;
	std::vector<std::pair<BlockPos, int64_t>> _torchToggles;
	int						_redstoneBlock, _power;
	JavaRandom				_soundSeeds{0};
	std::vector<bool>		_hasBlockEntity; // Per block: an EntityBlock
	std::shared_ptr<FuelValues>	   _fuelValues;	   // Furnaces and brewing stands
	std::shared_ptr<PotionBrewing> _potionBrewing;

	// Block entity tickers in vanilla's order: one slot per position, kept when its block entity is replaced
	struct TickerSlot {
		BlockPos pos;
		bool	 alive = true;
	};
	std::unordered_map<int64_t, std::shared_ptr<TickerSlot>> _tickerSlots;
	std::vector<std::shared_ptr<TickerSlot>>				  _tickers, _pendingTickers;
	bool													  _tickingBlockEntities = false;
	// Removed block entities, deleted once nothing can be using them anymore
	std::vector<std::shared_ptr<BlockEntity>> _removedBlockEntities;
	std::function<void(PistonMovingBlockEntity&)> _tickMovingPiston, _finalTickPiston;
	LootTables				_loot;
	MobRegistry				_mobs;
	EntityManager			_entities{*this}; // Last: its entities use the rest while being destroyed

	static constexpr uint8_t RANDOM_BLOCK = 1, RANDOM_FLUID = 2;

	Chunk* chunkAt(int chunkX, int chunkZ);
	void   countRandomTicking(Chunk& chunk);
	void   tickChunk(Chunk& chunk, int speed);
	void   attach(const std::shared_ptr<Chunk>& chunk);
	void   detach(int64_t key);
	// LevelChunk.setBlockState: the previous state, -1 if nothing changed
	int	 setBlockInChunk(Chunk& chunk, const BlockPos& pos, int state, int flags);
	bool sectionHasOnlyAir(const Chunk& chunk, int y) const;
	bool lightPropertiesDiffer(int a, int b) const;
	int	 lightAt(const BlockPos& pos, bool sky);
  public:
	// Block.updateOrDestroy: air breaks the block (with its drops unless UPDATE_SUPPRESS_DROPS), anything else replaces it
	void updateOrDestroy(int state, int updated, const BlockPos& pos, int flags, int limit = UPDATE_LIMIT);
	// Block.updateFromNeighbourShapes: the state after asking all six neighbors
	int	 updateFromNeighbourShapes(int state, const BlockPos& pos);

  private:
	// Block entity side effects of a block change (LevelChunk.setBlockState): the old one goes, the new block's comes
	void removeBlockEntityOnChange(const BlockPos& pos, int oldState, int newState, int flags);
	void createBlockEntityOnChange(const BlockPos& pos, int state);
	void addTicker(const BlockPos& pos);
	void removeTicker(const BlockPos& pos);
	void markChanged(const BlockPos& pos);
	void tickBlock(const BlockPos& pos, int block);
	void tickFluid(const BlockPos& pos, int fluid);
	// Block event packet to the players within 64 blocks
	void sendBlockEvent(const BlockEvent& event);
};

#endif
