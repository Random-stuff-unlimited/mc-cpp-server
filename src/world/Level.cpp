#include "world/Level.hpp"

#include "PacketIds.hpp"
#include "data/GameData.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Chunk.hpp"
#include "world/Shapes.hpp"
#include "world/ChunkStreamer.hpp"
#include "world/blocks/VanillaBlocks.hpp"
#include "world/entity/ItemEntity.hpp"
#include "world/item/FuelValues.hpp"
#include "world/item/PotionBrewing.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace {
	constexpr int	 LEVEL_EVENT_BLOCK_BREAK = 2001;
	constexpr double EVENT_RADIUS			 = 64.0; // PlayerList.broadcast distance of block and level events

	// SectionPos.asLong: x and z on 22 bits, y on 20
	int64_t sectionKey(int sectionX, int sectionY, int sectionZ) {
		return static_cast<int64_t>((static_cast<uint64_t>(sectionX) & 0x3FFFFF) << 42 | (static_cast<uint64_t>(sectionZ) & 0x3FFFFF) << 20 |
									(static_cast<uint64_t>(sectionY) & 0xFFFFF));
	}
	int sectionKeyX(int64_t key) { return static_cast<int>(key >> 42); }
	int sectionKeyY(int64_t key) { return static_cast<int>((key << 44) >> 44); }
	int sectionKeyZ(int64_t key) { return static_cast<int>((key << 22) >> 42); }

	bool withinEventRadius(const Player& player, const BlockPos& pos) {
		double dx = pos.x - player.getX(), dy = pos.y - player.getY(), dz = pos.z - player.getZ();
		return dx * dx + dy * dy + dz * dz < EVENT_RADIUS * EVENT_RADIUS;
	}
} // namespace

Level::Level(Server& server, World& world, const GameData& gameData)
	: _server(server), _world(world), _gameData(gameData), _blocks(gameData.getBlocks()),
	  _behaviors(gameData.getBlockCount()),
	  _neighborUpdater(*this), _blockTicks([this](int64_t key) {
		  auto it = _chunks.find(key);
		  return it != _chunks.end() && !it->second->isUnloaded() && it->second->isTicking();
	  }),
	  _fluidTicks([this](int64_t key) {
		  auto it = _chunks.find(key);
		  return it != _chunks.end() && !it->second->isUnloaded() && it->second->isTicking();
	  }),
	  _minY(world.getMinY()), _maxY(world.getMinY() + world.getSectionCount() * 16),
	  _random(std::chrono::steady_clock::now().time_since_epoch().count()), _mobs(gameData) {
	_recipes.load(gameData.getDirectory() / "recipes.json", gameData.getDirectory().parent_path() / "recipes", gameData);
	auto block		= [&](const char* name) { return gameData.getStaticId("minecraft:block", name); };
	_voidAir		= gameData.getDefaultBlockState("minecraft:void_air");
	_air			= gameData.getDefaultBlockState("minecraft:air");
	_comparator		= block("minecraft:comparator");
	_redstoneWire	= block("minecraft:redstone_wire");
	_fire			= block("minecraft:fire");
	_soulFire		= block("minecraft:soul_fire");
	_waterlogged	= _blocks.property("waterlogged");
	_simpleWaterlogged.assign(gameData.getBlockCount(), false);
	for (int id = 0; id < static_cast<int>(gameData.getBlockCount()); id++) _simpleWaterlogged[id] = gameData.isInstanceOf(id, "SimpleWaterloggedBlock");
	_ultraWarm		   = world.getDimensionName() == "minecraft:the_nether";
	_slimeBlock		   = block("minecraft:slime_block");
	_waterBlockId	   = block("minecraft:water");
	_bubbleColumnBlock = block("minecraft:bubble_column");
	_recipes.load(gameData.getDirectory() / "recipes.json", gameData.getDirectory().parent_path() / "recipes", gameData);
	_cactusBlock	   = block("minecraft:cactus");
	_loot.load(gameData.getDirectory() / "block_loot_tables.json", gameData);
	_loot.loadEntities(gameData.getDirectory() / "entity_loot_tables.json");
	registerVanillaMobs(_mobs);
	_fluids = std::make_unique<Fluids>(*this, gameData, _ultraWarm);
	registerVanillaBlocks(*this, gameData);
	_randValue = _random.nextInt();
	_redstoneBlock	   = block("minecraft:redstone_block");
	_power			   = _blocks.property("power");
	_hasBlockEntity.assign(gameData.getBlockCount(), false);
	for (int b = 0; b < static_cast<int>(gameData.getBlockCount()); b++) _hasBlockEntity[b] = gameData.isInstanceOf(b, "EntityBlock");

	// FluidState.isRandomlyTicking: lava only (it sets fire around)
	_randomTicks.resize(gameData.getBlockStateCount());
	for (size_t state = 0; state < _randomTicks.size(); state++) {
		const GameData::StateProperties& properties = gameData.getStateProperties(static_cast<int>(state));
		_randomTicks[state] = (properties.randomTicking ? RANDOM_BLOCK : 0) | (_fluids->isLava(_fluids->stateOf(static_cast<int>(state)).type) ? RANDOM_FLUID : 0);
	}
}

// ----- Chunks -----

Chunk* Level::chunkAt(int chunkX, int chunkZ) {
	int64_t key = Chunk::key(chunkX, chunkZ);
	if (_lastChunk && key == _lastKey && !_lastChunk->isUnloaded()) return _lastChunk;

	auto it = _chunks.find(key);
	if (it != _chunks.end()) {
		if (!it->second->isUnloaded()) {
			_lastKey   = key;
			_lastChunk = it->second.get();
			return _lastChunk;
		}
		detach(key);
	}
	// Loaded meanwhile, the load notification not handled yet
	std::shared_ptr<Chunk> chunk = _world.loadedChunk(chunkX, chunkZ);
	if (!chunk) return nullptr;
	attach(chunk);
	_lastKey   = key;
	_lastChunk = chunk.get();
	return _lastChunk;
}

void Level::attach(const std::shared_ptr<Chunk>& chunk) {
	int64_t key = Chunk::key(chunk->x(), chunk->z());
	auto	it	= _chunks.find(key);
	if (it != _chunks.end()) {
		if (it->second == chunk) return;
		detach(key);
	}
	chunk->blockTicks().unpack(getGameTime());
	chunk->fluidTicks().unpack(getGameTime());
	_blockTicks.addContainer(key, &chunk->blockTicks());
	_fluidTicks.addContainer(key, &chunk->fluidTicks());
	countRandomTicking(*chunk);
	_chunks[key] = chunk;
	// Its block entities (saved moving pistons tick again)
	for (auto& [index, entity] : chunk->blockEntities()) {
		entity->setLevel(this);
		if (entity->ticks()) addTicker(entity->pos());
	}
	// Its saved entities join at the start of the next entity phase
	std::vector<uint8_t> saved;
	{
		std::lock_guard<std::mutex> lock(chunk->mutex());
		saved = chunk->savedEntities();
	}
	if (!saved.empty()) _entities.queueLoad(std::move(saved));
}

// LevelChunkSection.recalcBlockCounts, for the randomly ticking blocks and fluids only
void Level::countRandomTicking(Chunk& chunk) {
	std::vector<uint16_t>& counts = chunk.randomTickingCounts();
	counts.assign(chunk.sections().size(), 0);
	std::lock_guard<std::mutex> lock(chunk.mutex());
	for (size_t i = 0; i < counts.size(); i++) {
		const PalettedContainer& blocks = chunk.sections()[i].blocks;
		if (blocks.isSingleValue()) {
			counts[i] = _randomTicks[blocks.singleValue()] ? 4096 : 0;
			continue;
		}
		// Most sections have no such block at all: the palette says it without looking at every block
		if (!blocks.palette().empty() &&
			std::none_of(blocks.palette().begin(), blocks.palette().end(), [this](uint32_t state) { return _randomTicks[state] != 0; })) {
			continue;
		}
		uint16_t count = 0;
		for (uint32_t index = 0; index < 4096; index++) count += _randomTicks[blocks.get(index)] != 0;
		counts[i] = count;
	}
}

void Level::detach(int64_t key) {
	auto found = _chunks.find(key);
	if (found != _chunks.end()) {
		for (auto& [index, entity] : found->second->blockEntities()) removeTicker(entity->pos());
	}
	_blockTicks.removeContainer(key);
	_fluidTicks.removeContainer(key);
	_entities.unloadChunk(key);
	_chunks.erase(key);
	if (_lastKey == key) _lastChunk = nullptr;
}

void Level::onChunkLoaded(const std::shared_ptr<Chunk>& chunk) {
	if (!chunk->isUnloaded()) attach(chunk);
}

void Level::saveEntities() {
	_entities.processPendingLoads(); // A chunk's entities must be back in the level before it is saved again
	for (const auto& [key, chunk] : _chunks) {
		if (chunk->isUnloaded()) continue;
		std::vector<uint8_t>		encoded = _entities.encodeChunk(key);
		std::lock_guard<std::mutex> lock(chunk->mutex());
		if (encoded == chunk->savedEntities()) continue;
		chunk->savedEntities() = std::move(encoded);
		chunk->setDirty(true);
	}
}

void Level::dropUnloadedChunks() {
	std::vector<int64_t> unloaded;
	for (const auto& [key, chunk] : _chunks) {
		if (chunk->isUnloaded()) unloaded.push_back(key);
	}
	for (int64_t key : unloaded) detach(key);
}

// ----- Reading -----

int Level::getBlockState(const BlockPos& pos) {
	if (isOutsideBuildHeight(pos.y)) return _voidAir;
	Chunk* chunk = chunkAt(pos.chunkX(), pos.chunkZ());
	return chunk ? static_cast<int>(chunk->getBlock(pos.x & 15, pos.y, pos.z & 15)) : _voidAir;
}

int Level::lightAt(const BlockPos& pos, bool sky) {
	if (pos.y >= _maxY) return sky ? 15 : 0;
	if (pos.y < _minY) return 0;
	Chunk* chunk = chunkAt(pos.chunkX(), pos.chunkZ());
	if (!chunk) return 0;
	std::lock_guard<std::mutex>		 lock(chunk->mutex()); // The light of a chunk can be computed on an I/O thread
	const std::vector<SectionLight>& sections = sky ? chunk->light().sky : chunk->light().block;
	int								 section  = (pos.y - _minY) >> 4;
	if (section >= static_cast<int>(sections.size())) return sky ? 15 : 0; // Not lit yet
	return sections[section].get((pos.y & 15) << 8 | (pos.z & 15) << 4 | (pos.x & 15));
}

int Level::getRawBrightness(const BlockPos& pos, int skyDarken) { return std::max(lightAt(pos, true) - skyDarken, lightAt(pos, false)); }

bool Level::shouldTickBlocksAt(const BlockPos& pos) {
	Chunk* chunk = chunkAt(pos.chunkX(), pos.chunkZ());
	return chunk && chunk->isTicking();
}


// ----- Writing -----

bool Level::setBlock(const BlockPos& pos, int state, int flags, int limit) {
	if (isOutsideBuildHeight(pos.y)) return false;
	Chunk* chunk = chunkAt(pos.chunkX(), pos.chunkZ());
	if (!chunk) return false; // Vanilla would load it
	int old = setBlockInChunk(*chunk, pos, state, flags);
	if (old < 0) return false;

	if (getBlockState(pos) == state) {
		if (flags & UPDATE_CLIENTS) markChanged(pos);
		if (flags & UPDATE_NEIGHBORS) {
			updateNeighborsAt(pos, _blocks.blockOf(old));
			if (_gameData.getStateProperties(state).analogOutput) updateNeighbourForOutputSignal(pos, _blocks.blockOf(state));
		}
		if (!(flags & UPDATE_KNOWN_SHAPE) && limit > 0) {
			int shapeFlags = flags & ~(UPDATE_NEIGHBORS | UPDATE_SUPPRESS_DROPS);
			behavior(old).updateIndirectNeighbourShapes(*this, pos, old, shapeFlags, limit - 1);
			updateNeighbourShapes(pos, state, shapeFlags, limit - 1);
			behavior(state).updateIndirectNeighbourShapes(*this, pos, state, shapeFlags, limit - 1);
		}
	}
	return true;
}

int Level::setBlockInChunk(Chunk& chunk, const BlockPos& pos, int state, int flags) {
	if (sectionHasOnlyAir(chunk, pos.y) && _blocks.isAir(state)) return -1;
	int old;
	{
		// The only lock of a block change: for the I/O threads reading the chunk
		std::lock_guard<std::mutex> lock(chunk.mutex());
		old = static_cast<int>(chunk.getBlock(pos.x & 15, pos.y, pos.z & 15));
		if (old == state) return -1;
		chunk.setBlock(pos.x & 15, pos.y, pos.z & 15, static_cast<uint32_t>(state)); // Marks it for saving
	}
	std::vector<uint16_t>& counts = chunk.randomTickingCounts();
	if (!counts.empty()) {
		uint16_t& count = counts[(pos.y - _minY) >> 4];
		count			= count - (_randomTicks[old] != 0) + (_randomTicks[state] != 0);
	}
	if (lightPropertiesDiffer(old, state)) _lightChecks.insert(pos.asLong());

	removeBlockEntityOnChange(pos, old, state, flags);
	int	 oldBlock	   = _blocks.blockOf(old);
	int	 newBlock	   = _blocks.blockOf(state);
	bool movedByPiston = flags & UPDATE_MOVE_BY_PISTON;
	// Vanilla also runs it when a rail changes shape (same block): added with the rails
	if (oldBlock != newBlock && ((flags & UPDATE_NEIGHBORS) || movedByPiston)) {
		_behaviors.get(oldBlock).affectNeighborsAfterRemoval(*this, pos, old, movedByPiston);
	}
	// Replaced meanwhile by the removal's side effects
	if (_blocks.blockOf(static_cast<int>(chunk.getBlock(pos.x & 15, pos.y, pos.z & 15))) != newBlock) return -1;
	if (!(flags & UPDATE_SKIP_ON_PLACE)) _behaviors.get(newBlock).onPlace(*this, pos, state, old, movedByPiston);
	createBlockEntityOnChange(pos, state);
	return old;
}

// Vanilla counts the non-air blocks of each section; the palette tells the same, except in the rare section whose
// palette still lists a block that was replaced by air
bool Level::sectionHasOnlyAir(const Chunk& chunk, int y) const {
	const PalettedContainer& blocks = chunk.sections()[(y - _minY) >> 4].blocks;
	if (blocks.isSingleValue()) return _blocks.isAir(static_cast<int>(blocks.singleValue()));
	if (blocks.palette().empty()) return false;
	return std::all_of(blocks.palette().begin(), blocks.palette().end(), [this](uint32_t state) { return _blocks.isAir(static_cast<int>(state)); });
}

bool Level::lightPropertiesDiffer(int a, int b) const {
	const GameData::StateProperties& first	= _gameData.getStateProperties(a);
	const GameData::StateProperties& second = _gameData.getStateProperties(b);
	return first.lightEmission != second.lightEmission || first.lightBlock != second.lightBlock || first.occludes != second.occludes ||
		   first.propagatesSkylightDown != second.propagatesSkylightDown;
}

bool Level::removeBlock(const BlockPos& pos, bool movedByPiston) {
	return setBlock(pos, fluidLegacyBlock(getBlockState(pos)), UPDATE_ALL | (movedByPiston ? UPDATE_MOVE_BY_PISTON : 0));
}

bool Level::destroyBlock(const BlockPos& pos, bool drop, int limit) {
	int state = getBlockState(pos);
	if (_blocks.isAir(state)) return false;
	int block = _blocks.blockOf(state);
	if (block != _fire && block != _soulFire) levelEvent(nullptr, LEVEL_EVENT_BLOCK_BREAK, pos, state);
	if (drop) dropResources(state, pos);
	return setBlock(pos, fluidLegacyBlock(state), UPDATE_ALL, limit);
}

void Level::dropResources(int state, const BlockPos& pos, Player* breaker, const ItemStack* tool) {
	LootTables::Context context{*this, pos, state, tool, breaker != nullptr, 0.0f};
	context.blockEntity = getBlockEntity(pos);
	for (auto it = _removedBlockEntities.rbegin(); !context.blockEntity && it != _removedBlockEntities.rend(); ++it) {
		if ((*it)->pos() == pos) context.blockEntity = it->get();
	}
	for (ItemStack& stack : _loot.blockDrops(context)) popResource(pos, std::move(stack));
}

void Level::popResource(const BlockPos& pos, ItemStack stack) {
	if (stack.isEmpty()) return;
	// Mth.nextDouble(random, -0.25, 0.25) on each axis; the item's center is half its height (0.125) lower
	double x = pos.x + 0.5 + (_random.nextDouble() * 0.5 - 0.25);
	double y = pos.y + 0.5 + (_random.nextDouble() * 0.5 - 0.25) - 0.125;
	double z = pos.z + 0.5 + (_random.nextDouble() * 0.5 - 0.25);
	auto   item = ItemEntity::create(*this, {x, y, z}, std::move(stack));
	item->setPickupDelay(10); // setDefaultPickUpDelay
	_entities.add(std::move(item));
}

void Level::dropFromPlayer(Player& player, ItemStack stack, bool) {
	if (stack.isEmpty()) return;
	double eyeY = player.getY() + 1.62 - 0.3f;
	auto   item = ItemEntity::create(*this, {player.getX(), eyeY, player.getZ()}, std::move(stack));
	item->setPickupDelay(40);
	// Thrown where the player looks, with a little spread
	float sinPitch = Mth::sin(player.getPitch() * (static_cast<float>(M_PI) / 180.0f));
	float cosPitch = Mth::cos(player.getPitch() * (static_cast<float>(M_PI) / 180.0f));
	float sinYaw   = Mth::sin(player.getYaw() * (static_cast<float>(M_PI) / 180.0f));
	float cosYaw   = Mth::cos(player.getYaw() * (static_cast<float>(M_PI) / 180.0f));
	float angle	   = _random.nextFloat() * (static_cast<float>(M_PI) * 2.0f);
	float spread   = 0.02f * _random.nextFloat();
	float lift	   = _random.nextFloat();
	lift -= _random.nextFloat();
	item->setDeltaMovement({-sinYaw * cosPitch * 0.3f + std::cos(angle) * spread, -sinPitch * 0.3f + 0.1f + lift * 0.1f,
							cosYaw * cosPitch * 0.3f + std::sin(angle) * spread});
	_entities.add(std::move(item));
}

void Level::markChanged(const BlockPos& pos) {
	uint16_t local = static_cast<uint16_t>((pos.x & 15) << 8 | (pos.z & 15) << 4 | (pos.y & 15));
	_changedSections[sectionKey(pos.x >> 4, pos.y >> 4, pos.z >> 4)].push_back(local);
}

// ----- Updates -----

void Level::updateNeighborsAt(const BlockPos& pos, int sourceBlock) { _neighborUpdater.updateNeighborsAtExceptFromFacing(pos, sourceBlock, nullptr); }

void Level::updateNeighborsAtExceptFromFacing(const BlockPos& pos, int sourceBlock, Direction skip) {
	_neighborUpdater.updateNeighborsAtExceptFromFacing(pos, sourceBlock, &skip);
}

void Level::neighborChanged(const BlockPos& pos, int sourceBlock) { _neighborUpdater.neighborChanged(pos, sourceBlock); }

void Level::neighborChanged(int state, const BlockPos& pos, int sourceBlock, bool movedByPiston) {
	_neighborUpdater.neighborChanged(state, pos, sourceBlock, movedByPiston);
}

void Level::neighborShapeChanged(Direction direction, const BlockPos& pos, const BlockPos& neighborPos, int neighborState, int flags, int limit) {
	_neighborUpdater.shapeUpdate(direction, neighborState, pos, neighborPos, flags, limit);
}

void Level::updateNeighbourShapes(const BlockPos& pos, int state, int flags, int limit) {
	for (Direction direction : Directions::SHAPE_UPDATE_ORDER) {
		neighborShapeChanged(Directions::opposite(direction), pos.relative(direction), pos, state, flags, limit);
	}
}

void Level::updateNeighbourForOutputSignal(const BlockPos& pos, int sourceBlock) {
	for (Direction direction : Directions::HORIZONTAL) {
		BlockPos neighbor = pos.relative(direction);
		if (!hasChunkAt(neighbor)) continue;
		int state = getBlockState(neighbor);
		if (_blocks.blockOf(state) == _comparator) {
			neighborChanged(state, neighbor, sourceBlock, false);
		} else if (isRedstoneConductor(state)) {
			neighbor = neighbor.relative(direction);
			state	 = getBlockState(neighbor);
			if (_blocks.blockOf(state) == _comparator) neighborChanged(state, neighbor, sourceBlock, false);
		}
	}
}

// ----- Redstone -----

bool Level::isRedstoneConductor(int state) const { return _gameData.getStateProperties(state).redstoneConductor; }

int Level::getDirectSignal(const BlockPos& pos, Direction direction) {
	int state = getBlockState(pos);
	return behavior(state).getDirectSignal(*this, pos, state, direction);
}

int Level::getDirectSignalTo(const BlockPos& pos) {
	int signal = 0;
	for (Direction direction : Directions::ALL) {
		signal = std::max(signal, getDirectSignal(pos.relative(direction), direction));
		if (signal >= 15) return signal;
	}
	return signal;
}

int Level::getControlInputSignal(const BlockPos& pos, Direction direction, bool diodesOnly) {
	int state = getBlockState(pos);
	int block = _blocks.blockOf(state);
	if (diodesOnly) return _gameData.isInstanceOf(block, "DiodeBlock") ? getDirectSignal(pos, direction) : 0;
	if (block == _redstoneBlock) return 15;
	if (block == _redstoneWire) return _blocks.getInt(state, _power);
	return isSignalSource(state) ? getDirectSignal(pos, direction) : 0;
}

int Level::getSignal(const BlockPos& pos, Direction direction) {
	int state  = getBlockState(pos);
	int signal = behavior(state).getSignal(*this, pos, state, direction);
	return isRedstoneConductor(state) ? std::max(signal, getDirectSignalTo(pos)) : signal;
}

bool Level::hasNeighborSignal(const BlockPos& pos) {
	for (Direction direction : Directions::ALL) {
		if (getSignal(pos.relative(direction), direction) > 0) return true;
	}
	return false;
}

int Level::getBestNeighborSignal(const BlockPos& pos) {
	int best = 0;
	for (Direction direction : Directions::ALL) {
		int signal = getSignal(pos.relative(direction), direction);
		if (signal >= 15) return 15;
		best = std::max(best, signal);
	}
	return best;
}

// ----- Block entities -----

BlockEntity* Level::getBlockEntity(const BlockPos& pos) { return getSharedBlockEntity(pos).get(); }

std::shared_ptr<BlockEntity> Level::getSharedBlockEntity(const BlockPos& pos) {
	if (isOutsideBuildHeight(pos.y)) return nullptr;
	Chunk* chunk = chunkAt(pos.chunkX(), pos.chunkZ());
	if (!chunk) return nullptr;
	auto it = chunk->blockEntities().find(chunk->indexOf(pos.x, pos.y, pos.z));
	return it == chunk->blockEntities().end() || it->second->isRemoved() ? nullptr : it->second;
}

void Level::setBlockEntity(std::unique_ptr<BlockEntity> entity) {
	BlockPos pos   = entity->pos();
	Chunk*	 chunk = chunkAt(pos.chunkX(), pos.chunkZ());
	if (!chunk) return;
	entity->setLevel(this);
	bool ticks = entity->ticks();
	{
		std::lock_guard<std::mutex> lock(chunk->mutex());
		std::shared_ptr<BlockEntity>& slot = chunk->blockEntities()[chunk->indexOf(pos.x, pos.y, pos.z)];
		if (slot) {
			slot->setRemoved();
			_removedBlockEntities.push_back(std::move(slot));
		}
		slot = std::move(entity);
		chunk->setDirty(true);
	}
	if (ticks) {
		addTicker(pos);
	} else {
		removeTicker(pos);
	}
}

void Level::removeBlockEntity(const BlockPos& pos) {
	Chunk* chunk = chunkAt(pos.chunkX(), pos.chunkZ());
	if (!chunk) return;
	{
		std::lock_guard<std::mutex> lock(chunk->mutex());
		auto						it = chunk->blockEntities().find(chunk->indexOf(pos.x, pos.y, pos.z));
		if (it == chunk->blockEntities().end()) return;
		it->second->setRemoved();
		_removedBlockEntities.push_back(std::move(it->second));
		chunk->blockEntities().erase(it);
		chunk->setDirty(true);
	}
	removeTicker(pos);
}

void Level::blockEntityChanged(const BlockPos& pos) {
	Chunk* chunk = chunkAt(pos.chunkX(), pos.chunkZ());
	if (!chunk) return;
	chunk->setDirty(true);
	int state = getBlockState(pos);
	if (!_blocks.isAir(state)) updateNeighbourForOutputSignal(pos, _blocks.blockOf(state));
}

int Level::comparatorOutput(const BlockPos& pos) {
	auto* comparator = getBlockEntity<ComparatorBlockEntity>(pos);
	return comparator ? comparator->output : 0;
}

void Level::setComparatorOutput(const BlockPos& pos, int output) {
	auto* comparator = getBlockEntity<ComparatorBlockEntity>(pos);
	if (!comparator) return;
	comparator->output = output;
	if (Chunk* chunk = chunkAt(pos.chunkX(), pos.chunkZ())) chunk->setDirty(true);
}

// LevelChunk.updateBlockEntityTicker: a new ticking block entity keeps the slot of the one it replaces
void Level::addTicker(const BlockPos& pos) {
	int64_t key = pos.asLong();
	if (_tickerSlots.count(key)) return;
	auto slot		  = std::make_shared<TickerSlot>();
	slot->pos		  = pos;
	_tickerSlots[key] = slot;
	(_tickingBlockEntities ? _pendingTickers : _tickers).push_back(slot);
}

void Level::removeTicker(const BlockPos& pos) {
	auto slot = _tickerSlots.find(pos.asLong());
	if (slot == _tickerSlots.end()) return;
	slot->second->alive = false;
	_tickerSlots.erase(slot);
}

void Level::tickBlockEntities() {
	_tickingBlockEntities = true;
	_tickers.insert(_tickers.end(), _pendingTickers.begin(), _pendingTickers.end());
	_pendingTickers.clear();
	for (size_t i = 0; i < _tickers.size();) {
		std::shared_ptr<TickerSlot> slot = _tickers[i];
		if (!slot->alive) {
			_tickers.erase(_tickers.begin() + static_cast<std::ptrdiff_t>(i));
			continue;
		}
		i++;
		// BoundTickingBlockEntity: only in a ticking chunk
		if (!shouldTickBlocksAt(slot->pos)) continue;
		BlockEntity* entity = getBlockEntity(slot->pos);
		if (entity && entity->ticks()) entity->tick(*this);
	}
	_tickingBlockEntities = false;
	_removedBlockEntities.clear();
}

// LevelChunk.setBlockState: another block, the block entity goes (with its side effects, like a moving piston ending
// or a container dropping its items)
void Level::removeBlockEntityOnChange(const BlockPos& pos, int oldState, int newState, int flags) {
	int oldBlock = _blocks.blockOf(oldState), newBlock = _blocks.blockOf(newState);
	if (oldBlock == newBlock || !_hasBlockEntity[oldBlock] || behavior(newState).keepsBlockEntityOf(oldState)) return;
	if (!(flags & UPDATE_SKIP_BLOCK_ENTITY_SIDEEFFECTS)) {
		if (BlockEntity* entity = getBlockEntity(pos)) entity->preRemoveSideEffectsWithState(*this, oldState);
	}
	removeBlockEntity(pos);
}

// And the new block's own, if it has one and there is none yet
void Level::createBlockEntityOnChange(const BlockPos& pos, int state) {
	if (!_hasBlockEntity[_blocks.blockOf(state)] || getBlockEntity(pos)) return;
	std::unique_ptr<BlockEntity> entity = behavior(state).newBlockEntity(pos, state);
	if (!entity) entity = BlockEntity::create(_gameData.getBlockProperties(state).blockEntity, pos);
	if (entity) setBlockEntity(std::move(entity));
}

// ----- Sounds and entities -----

void Level::playSoundAt(Player* except, double x, double y, double z, const std::string& sound, SoundSource source, float volume, float pitch) {
	int id = _gameData.getStaticId("minecraft:sound_event", sound);
	if (id < 0) return;
	Buffer data;
	data.writeVarInt(id + 1); // A registry id (0: sound given inline)
	data.writeVarInt(static_cast<int>(source));
	data.writeInt(static_cast<int32_t>(x * 8.0));
	data.writeInt(static_cast<int32_t>(y * 8.0));
	data.writeInt(static_cast<int32_t>(z * 8.0));
	data.writeFloat(volume);
	data.writeFloat(pitch);
	data.writeLong(static_cast<int64_t>(_soundSeeds.nextLong())); // Vanilla's threadSafeRandom: not the level's random
	// PlayerList.broadcast: 16 blocks, more for louder sounds
	double				 range = volume > 1.0F ? 16.0 * volume : 16.0;
	std::vector<uint8_t> frame;
	for (const auto& player : _server.getGamePlayers()) {
		if (player.get() == except) continue;
		double dx = x - player->getX(), dy = y - player->getY(), dz = z - player->getZ();
		if (dx * dx + dy * dy + dz * dz >= range * range) continue;
		if (frame.empty()) frame = Packet::buildFrame(PacketId::Play::Clientbound::SOUND, data.getData(), _server.getConfig().getCompressionThreshold());
		Packet::sendFrame(player, frame, _server);
	}
}

int Level::countEntities(const AABB& box, bool livingOnly) {
	int count = 0;
	for (const auto& player : _server.getGamePlayers()) {
		if (player->isDisconnected() || player->getGameMode() == GameMode::Spectator || player->combat().dead) continue;
		double half = Player::BB_WIDTH / 2.0;
		AABB   playerBox{player->getX() - half, player->getY(), player->getZ() - half, player->getX() + half, player->getY() + Player::BB_HEIGHT,
						 player->getZ() + half};
		if (playerBox.intersects(box)) count++;
	}
	_entities.forEachIn(box, [&](Entity& entity) { count += !livingOnly || entity.isLiving(); });
	return count;
}

bool Level::hasBlockCollision(const AABB& box) {
	for (int x = Mth::floor(box.minX) - 1; x <= Mth::floor(box.maxX) + 1; x++) {
		for (int y = Mth::floor(box.minY) - 1; y <= Mth::floor(box.maxY) + 1; y++) {
			for (int z = Mth::floor(box.minZ) - 1; z <= Mth::floor(box.maxZ) + 1; z++) {
				for (const GameData::Box& b : _gameData.getCollisionShape(getBlockState({x, y, z}))) {
					if (AABB{x + b.minX, y + b.minY, z + b.minZ, x + b.maxX, y + b.maxY, z + b.maxZ}.intersects(box)) return true;
				}
			}
		}
	}
	return false;
}

void Level::checkInsideBlocks(const AABB& box, Entity* entity) {
	AABB inside = box.deflate(1.0E-5);
	for (int x = Mth::floor(inside.minX); x <= Mth::floor(inside.maxX); x++) {
		for (int y = Mth::floor(inside.minY); y <= Mth::floor(inside.maxY); y++) {
			for (int z = Mth::floor(inside.minZ); z <= Mth::floor(inside.maxZ); z++) {
				BlockPos pos{x, y, z};
				int		 state = getBlockState(pos);
				if (!_blocks.isAir(state)) behavior(state).entityInside(*this, pos, state, entity);
			}
		}
	}
}

void Level::executeNeighborChanged(int state, const BlockPos& pos, int sourceBlock, bool movedByPiston) {
	behavior(state).neighborChanged(*this, pos, state, sourceBlock, movedByPiston);
}

void Level::executeShapeUpdate(Direction direction, const BlockPos& pos, const BlockPos& neighborPos, int neighborState, int flags, int limit) {
	int state = getBlockState(pos);
	if ((flags & UPDATE_SKIP_SHAPE_UPDATE_ON_WIRE) && _blocks.blockOf(state) == _redstoneWire) return;
	int updated = behavior(state).updateShape(*this, pos, state, direction, neighborPos, neighborState);
	updateOrDestroy(state, updated, pos, flags, limit);
}

// Block.updateOrDestroy
void Level::updateOrDestroy(int state, int updated, const BlockPos& pos, int flags, int limit) {
	if (updated == state) return;
	if (_blocks.isAir(updated)) {
		destroyBlock(pos, !(flags & UPDATE_SUPPRESS_DROPS), limit);
	} else {
		setBlock(pos, updated, flags & ~UPDATE_SUPPRESS_DROPS, limit);
	}
}

int Level::updateFromNeighbourShapes(int state, const BlockPos& pos) {
	for (Direction direction : Directions::SHAPE_UPDATE_ORDER) {
		BlockPos neighbor = pos.relative(direction);
		state			  = behavior(state).updateShape(*this, pos, state, direction, neighbor, getBlockState(neighbor));
	}
	return state;
}

// ----- Scheduled ticks and events -----

void Level::scheduleTick(const BlockPos& pos, int block, int delay, int priority) {
	chunkAt(pos.chunkX(), pos.chunkZ()); // Its ticks must be known
	_blockTicks.schedule({block, pos, getGameTime() + delay, priority, _subTickCount++});
}

void Level::scheduleFluidTick(const BlockPos& pos, int fluid, int delay, int priority) {
	chunkAt(pos.chunkX(), pos.chunkZ());
	_fluidTicks.schedule({fluid, pos, getGameTime() + delay, priority, _subTickCount++});
}

void Level::tickScheduled() {
	_blockTicks.tick(getGameTime(), MAX_TICKS_PER_TICK, [this](const BlockPos& pos, int block) { tickBlock(pos, block); });
	_fluidTicks.tick(getGameTime(), MAX_TICKS_PER_TICK, [this](const BlockPos& pos, int fluid) { tickFluid(pos, fluid); });
}

void Level::updateSkyBrightness() {
	// DimensionType.timeOfDay: the nether and the end have a fixed time
	int64_t time = _world.getDayTime();
	if (_world.getDimensionName() == "minecraft:the_nether") time = 18000;
	if (_world.getDimensionName() == "minecraft:the_end") time = 6000;
	double frac		 = time / 24000.0 - 0.25;
	frac			-= std::floor(frac);
	double eased	 = 0.5 - std::cos(frac * M_PI) / 2.0;
	float  timeOfDay = static_cast<float>(frac * 2.0 + eased) / 3.0F;
	// No weather yet: rain and thunder levels are 0
	double light = 0.5 + 2.0 * std::clamp(static_cast<double>(Mth::cos(timeOfDay * static_cast<float>(M_PI * 2))), -0.25, 0.25);
	_skyDarken	 = static_cast<int>((1.0 - light) * 11.0);
}

BlockPos Level::getBlockRandomPos(int x, int y, int z, int yMask) {
	_randValue = static_cast<int>(static_cast<uint32_t>(_randValue) * 3u + 1013904223u);
	int value  = _randValue >> 2;
	return {x + (value & 15), y + (value >> 16 & yMask), z + (value >> 8 & 15)};
}

int Level::getLightBlockInto(int from, int to, Direction direction, int lightBlock) const {
	// isEmptyShape: blocks that don't occlude, or not with their shape
	auto empty = [this](int state) {
		const GameData::StateProperties& properties = _gameData.getStateProperties(state);
		return !properties.occludes || !properties.useShapeForLightOcclusion;
	};
	bool fromEmpty = empty(from), toEmpty = empty(to);
	if (fromEmpty && toEmpty) return lightBlock;
	static const std::vector<GameData::Box> none;
	const std::vector<GameData::Box>&		first  = fromEmpty ? none : _gameData.getOcclusionShape(from);
	const std::vector<GameData::Box>&		second = toEmpty ? none : _gameData.getOcclusionShape(to);
	return Shapes::mergedFaceOccludes(first, second, direction) ? 16 : lightBlock;
}

void Level::tickChunks() {
	if (_randomTickSpeed <= 0) return;
	for (const auto& [key, chunk] : _chunks) {
		if (!chunk->isUnloaded() && chunk->isTicking()) tickChunk(*chunk, _randomTickSpeed);
	}
}

void Level::tickChunk(Chunk& chunk, int speed) {
	int minX = chunk.x() * 16, minZ = chunk.z() * 16;
	// Snow and ice with the weather (not there yet): the random numbers are drawn all the same
	for (int i = 0; i < speed; i++) {
		if (_random.nextInt(48) == 0) getBlockRandomPos(minX, 0, minZ, 15);
	}
	std::vector<uint16_t>& counts = chunk.randomTickingCounts();
	for (size_t section = 0; section < counts.size(); section++) {
		if (counts[section] == 0) continue;
		int minY = _minY + static_cast<int>(section) * 16;
		for (int i = 0; i < speed; i++) {
			BlockPos pos   = getBlockRandomPos(minX, minY, minZ, 15);
			int		 state = static_cast<int>(chunk.getBlock(pos.x & 15, pos.y, pos.z & 15));
			if (_randomTicks[state] & RANDOM_BLOCK) _behaviors.randomTicker(_blocks.blockOf(state)).randomTick(*this, pos, state);
			if (_randomTicks[state] & RANDOM_FLUID) _fluids->randomTick(pos, state);
		}
	}
}

void Level::tickBlock(const BlockPos& pos, int block) {
	int state = getBlockState(pos);
	if (_blocks.blockOf(state) == block) _behaviors.get(block).tick(*this, pos, state);
}

void Level::scheduleWaterlogged(const BlockPos& pos, int state) {
	if (_simpleWaterlogged[_blocks.blockOf(state)] && _blocks.getBool(state, _waterlogged)) {
		scheduleFluidTick(pos, _fluids->water(), _fluids->tickDelay(_fluids->water()));
	}
}

void Level::tickFluid(const BlockPos& pos, int fluid) {
	int		   state	  = getBlockState(pos);
	FluidState fluidState = _fluids->stateOf(state);
	if (fluidState.type == fluid) _fluids->tick(pos, state, fluidState);
}

void Level::blockEvent(const BlockPos& pos, int block, int type, int data) {
	BlockEvent event{pos, block, type, data};
	if (_blockEventSet.insert(event).second) _blockEvents.push_back(event);
}

// Events of chunks that don't tick wait for them; events added while running run in the same pass
void Level::runBlockEvents() {
	std::vector<BlockEvent> reschedule;
	while (!_blockEvents.empty()) {
		BlockEvent event = _blockEvents.front();
		_blockEvents.pop_front();
		_blockEventSet.erase(event);
		if (!shouldTickBlocksAt(event.pos)) {
			reschedule.push_back(event);
			continue;
		}
		int state = getBlockState(event.pos);
		if (_blocks.blockOf(state) == event.block && _behaviors.get(event.block).triggerEvent(*this, event.pos, state, event.type, event.data)) {
			sendBlockEvent(event);
		}
	}
	for (const BlockEvent& event : reschedule) blockEvent(event.pos, event.block, event.type, event.data);
}

void Level::sendBlockEvent(const BlockEvent& event) {
	Buffer data;
	data.writePosition(event.pos.x, event.pos.y, event.pos.z);
	data.writeUByte(static_cast<uint8_t>(event.type));
	data.writeUByte(static_cast<uint8_t>(event.data));
	data.writeVarInt(event.block);
	std::vector<uint8_t> frame;
	for (const auto& player : _server.getGamePlayers()) {
		if (!withinEventRadius(*player, event.pos)) continue;
		if (frame.empty()) frame = Packet::buildFrame(PacketId::Play::Clientbound::BLOCK_EVENT, data.getData(), _server.getConfig().getCompressionThreshold());
		Packet::sendFrame(player, frame, _server);
	}
}

void Level::levelEvent(Player* except, int type, const BlockPos& pos, int eventData) {
	Buffer data;
	data.writeInt(type);
	data.writePosition(pos.x, pos.y, pos.z);
	data.writeInt(eventData);
	data.writeBool(false); // Not global
	std::vector<uint8_t> frame;
	for (const auto& player : _server.getGamePlayers()) {
		if (player.get() == except || !withinEventRadius(*player, pos)) continue;
		if (frame.empty()) frame = Packet::buildFrame(PacketId::Play::Clientbound::LEVEL_EVENT, data.getData(), _server.getConfig().getCompressionThreshold());
		Packet::sendFrame(player, frame, _server);
	}
}

// ----- Sending the changes -----

// Like vanilla's ChunkHolder.broadcastChanges: the light first, then the blocks as they are now
void Level::sendChanges() {
	if (!_lightChecks.empty()) {
		World::LightChanges light;
		for (int64_t packed : _lightChecks) {
			BlockPos pos = BlockPos::fromLong(packed);
			if (chunkAt(pos.chunkX(), pos.chunkZ())) _world.relight(pos.x, pos.y, pos.z, light);
		}
		_lightChecks.clear();
		for (const auto& [key, change] : light) {
			int	   chunkX = static_cast<int32_t>(key & 0xFFFFFFFF), chunkZ = static_cast<int32_t>(key >> 32);
			Chunk* chunk  = chunkAt(chunkX, chunkZ);
			if (!chunk) continue;
			Buffer packet(_world.lightUpdatePacket(*chunk, change));
			_server.broadcastToChunk(chunkX, chunkZ, PacketId::Play::Clientbound::LIGHT_UPDATE, packet);
		}
	}

	for (auto& [key, positions] : _changedSections) {
		std::sort(positions.begin(), positions.end());
		positions.erase(std::unique(positions.begin(), positions.end()), positions.end());
		int sectionX = sectionKeyX(key), sectionY = sectionKeyY(key), sectionZ = sectionKeyZ(key);
		auto stateAt = [&](uint16_t local) {
			return getBlockState({sectionX * 16 + (local >> 8), sectionY * 16 + (local & 15), sectionZ * 16 + ((local >> 4) & 15)});
		};
		Buffer packet;
		int	   packetId;
		if (positions.size() == 1) {
			uint16_t local = positions[0];
			packet.writePosition(sectionX * 16 + (local >> 8), sectionY * 16 + (local & 15), sectionZ * 16 + ((local >> 4) & 15));
			packet.writeVarInt(stateAt(local));
			packetId = PacketId::Play::Clientbound::BLOCK_UPDATE;
		} else {
			packet.writeLong(key);
			packet.writeVarInt(static_cast<int32_t>(positions.size()));
			for (uint16_t local : positions) packet.writeVarLong(static_cast<int64_t>(stateAt(local)) << 12 | local);
			packetId = PacketId::Play::Clientbound::SECTION_BLOCKS_UPDATE;
		}
		_server.broadcastToChunk(sectionX, sectionZ, packetId, packet);
		// ChunkHolder.broadcastBlockEntityIfNeeded: the changed blocks' block entities that have an update packet
		for (uint16_t local : positions) {
			BlockPos	 at{sectionX * 16 + (local >> 8), sectionY * 16 + (local & 15), sectionZ * 16 + ((local >> 4) & 15)};
			BlockEntity* entity = getBlockEntity(at);
			if (!entity || !entity->hasUpdatePacket()) continue;
			Buffer data;
			data.writePosition(at.x, at.y, at.z);
			data.writeVarInt(_gameData.getStaticId("minecraft:block_entity_type", entity->type()));
			std::vector<uint8_t> tag;
			entity->writeUpdateTag(tag);
			data.writeBytes(tag);
			_server.broadcastToChunk(sectionX, sectionZ, PacketId::Play::Clientbound::BLOCK_ENTITY_DATA, data);
		}
	}
	_changedSections.clear();
}

// ----- Furnaces and brewing stands -----

const FuelValues& Level::fuelValues() {
	if (!_fuelValues) _fuelValues = std::make_shared<FuelValues>(_gameData);
	return *_fuelValues;
}

const PotionBrewing& Level::potionBrewing() {
	if (!_potionBrewing) _potionBrewing = std::make_shared<PotionBrewing>(_gameData);
	return *_potionBrewing;
}
