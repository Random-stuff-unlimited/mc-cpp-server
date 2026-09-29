#include "world/PoiManager.hpp"

#include "data/GameData.hpp"
#include "logger.hpp"
#include "network/buffer.hpp"
#include "world/Chunk.hpp"

#include <algorithm>
#include <fstream>

namespace {
	int64_t chunkKeyOf(const BlockPos& pos) { return Chunk::key(pos.x >> 4, pos.z >> 4); }
	constexpr uint32_t FILE_MAGIC = 0x504F4931; // "POI1"
} // namespace

PoiManager::PoiManager(const GameData& gameData, std::filesystem::path file) : _file(std::move(file)) {
	_typeByState.assign(gameData.getBlockStateCount(), -1);
	auto blockType = [&](const std::string& block, Type type, const std::function<bool(int)>& filter = nullptr) {
		int id = gameData.getStaticId("minecraft:block", block);
		if (id < 0) return;
		for (int state = 0; state < gameData.getBlockStateCount(); state++) {
			if (gameData.getBlockOfState(state) == id && (!filter || filter(state))) _typeByState[state] = static_cast<int8_t>(type);
		}
	};
	blockType("minecraft:blast_furnace", Type::Armorer);
	blockType("minecraft:smoker", Type::Butcher);
	blockType("minecraft:cartography_table", Type::Cartographer);
	blockType("minecraft:brewing_stand", Type::Cleric);
	blockType("minecraft:composter", Type::Farmer);
	blockType("minecraft:barrel", Type::Fisherman);
	blockType("minecraft:fletching_table", Type::Fletcher);
	for (const char* cauldron : {"minecraft:cauldron", "minecraft:lava_cauldron", "minecraft:water_cauldron", "minecraft:powder_snow_cauldron"}) {
		blockType(cauldron, Type::Leatherworker);
	}
	blockType("minecraft:lectern", Type::Librarian);
	blockType("minecraft:stonecutter", Type::Mason);
	blockType("minecraft:loom", Type::Shepherd);
	blockType("minecraft:smithing_table", Type::Toolsmith);
	blockType("minecraft:grindstone", Type::Weaponsmith);
	// Beds: the head only
	for (const char* color : {"red", "black", "blue", "brown", "cyan", "gray", "green", "light_blue", "light_gray", "lime", "magenta", "orange", "pink",
							  "purple", "white", "yellow"}) {
		blockType(std::string("minecraft:") + color + "_bed", Type::Home, [&](int state) { return gameData.getProperty(state, "part") == "head"; });
	}
	blockType("minecraft:bell", Type::Meeting);
	blockType("minecraft:beehive", Type::Beehive);
	blockType("minecraft:bee_nest", Type::BeeNest);
	blockType("minecraft:nether_portal", Type::NetherPortal);
	blockType("minecraft:lodestone", Type::Lodestone);
	blockType("minecraft:test_instance_block", Type::TestInstance);
	for (const char* rod : {"minecraft:lightning_rod", "minecraft:exposed_lightning_rod", "minecraft:weathered_lightning_rod", "minecraft:oxidized_lightning_rod",
							"minecraft:waxed_lightning_rod", "minecraft:waxed_exposed_lightning_rod", "minecraft:waxed_weathered_lightning_rod",
							"minecraft:waxed_oxidized_lightning_rod"}) {
		blockType(rod, Type::LightningRod);
	}
	load();
}

int PoiManager::maxTickets(Type type) {
	switch (type) {
	case Type::Meeting: return 32;
	case Type::Beehive:
	case Type::BeeNest:
	case Type::NetherPortal:
	case Type::Lodestone:
	case Type::LightningRod:
	case Type::TestInstance: return 0;
	default: return 1;
	}
}

int PoiManager::validRange(Type type) { return type == Type::Meeting ? 6 : 1; }

void PoiManager::add(const BlockPos& pos, Type type) {
	int64_t key = pos.asLong();
	auto	it	= _byPos.find(key);
	if (it != _byPos.end()) {
		if (it->second.type == type) return;
		it->second = {pos, type, maxTickets(type)};
	} else {
		_byPos[key] = {pos, type, maxTickets(type)};
		_byChunk[chunkKeyOf(pos)].push_back(key);
	}
	_dirty = true;
}

void PoiManager::remove(const BlockPos& pos) {
	int64_t key = pos.asLong();
	if (!_byPos.erase(key)) return;
	auto chunk = _byChunk.find(chunkKeyOf(pos));
	if (chunk != _byChunk.end()) {
		auto& list = chunk->second;
		list.erase(std::remove(list.begin(), list.end(), key), list.end());
		if (list.empty()) _byChunk.erase(chunk);
	}
	_dirty = true;
}

void PoiManager::onBlockChanged(const BlockPos& pos, int oldState, int newState) {
	std::optional<Type> before = typeOf(oldState), after = typeOf(newState);
	if (before == after) return;
	if (before) remove(pos);
	if (after) add(pos, *after);
}

void PoiManager::onChunkLoaded(const Chunk& chunk) {
	// Positions the index has in this chunk that aren't the right block anymore go, blocks it lacks come
	int64_t key = Chunk::key(chunk.x(), chunk.z());
	std::vector<std::pair<BlockPos, Type>> found;
	const auto&							   sections = chunk.sections();
	for (size_t s = 0; s < sections.size(); s++) {
		const PalettedContainer& blocks = sections[s].blocks;
		bool					 any	= false;
		if (blocks.isSingleValue()) {
			any = typeOf(static_cast<int>(blocks.singleValue())).has_value();
		} else {
			for (uint32_t state : blocks.palette()) any |= typeOf(static_cast<int>(state)).has_value();
		}
		if (!any) continue;
		for (uint32_t index = 0; index < 4096; index++) {
			if (std::optional<Type> type = typeOf(static_cast<int>(blocks.get(index)))) {
				int x = chunk.x() * 16 + static_cast<int>(index & 15), z = chunk.z() * 16 + static_cast<int>(index >> 4 & 15);
				int y = chunk.minY() + static_cast<int>(s) * 16 + static_cast<int>(index >> 8);
				found.emplace_back(BlockPos{x, y, z}, *type);
			}
		}
	}
	auto existing = _byChunk.find(key);
	if (existing != _byChunk.end()) {
		std::vector<int64_t> positions = existing->second;
		for (int64_t pos : positions) {
			const Record& record = _byPos.at(pos);
			bool		  still	 = std::any_of(found.begin(), found.end(), [&](const auto& f) { return f.first == record.pos && f.second == record.type; });
			if (!still) remove(record.pos);
		}
	}
	for (const auto& [pos, type] : found) add(pos, type);
}

bool PoiManager::matches(const Record& record, Occupancy occupancy) {
	switch (occupancy) {
	case Occupancy::HasSpace: return record.freeTickets > 0;
	case Occupancy::IsOccupied: return record.freeTickets < maxTickets(record.type);
	case Occupancy::Any: return true;
	}
	return true;
}

std::vector<const PoiManager::Record*> PoiManager::getInSquare(Type type, const BlockPos& center, int radius, Occupancy occupancy) const {
	std::vector<const Record*> result;
	for (int cx = (center.x - radius) >> 4; cx <= (center.x + radius) >> 4; cx++) {
		for (int cz = (center.z - radius) >> 4; cz <= (center.z + radius) >> 4; cz++) {
			auto chunk = _byChunk.find(Chunk::key(cx, cz));
			if (chunk == _byChunk.end()) continue;
			for (int64_t key : chunk->second) {
				const Record& record = _byPos.at(key);
				if (record.type != type || !matches(record, occupancy)) continue;
				if (std::abs(record.pos.x - center.x) <= radius && std::abs(record.pos.z - center.z) <= radius) result.push_back(&record);
			}
		}
	}
	return result;
}

std::vector<const PoiManager::Record*> PoiManager::getInRange(Type type, const BlockPos& center, int radius, Occupancy occupancy) const {
	std::vector<const Record*> result;
	for (const Record* record : getInSquare(type, center, radius, occupancy)) {
		int64_t dx = record->pos.x - center.x, dy = record->pos.y - center.y, dz = record->pos.z - center.z;
		if (dx * dx + dy * dy + dz * dz <= static_cast<int64_t>(radius) * radius) result.push_back(record);
	}
	return result;
}

const PoiManager::Record* PoiManager::get(const BlockPos& pos) const {
	auto it = _byPos.find(pos.asLong());
	return it == _byPos.end() ? nullptr : &it->second;
}

bool PoiManager::take(const BlockPos& pos) {
	auto it = _byPos.find(pos.asLong());
	if (it == _byPos.end() || it->second.freeTickets <= 0) return false;
	it->second.freeTickets--;
	_dirty = true;
	return true;
}

bool PoiManager::release(const BlockPos& pos) {
	auto it = _byPos.find(pos.asLong());
	if (it == _byPos.end() || it->second.freeTickets >= maxTickets(it->second.type)) return false;
	it->second.freeTickets++;
	_dirty = true;
	return true;
}

std::vector<uint8_t> PoiManager::encode() const {
	Buffer out;
	out.writeInt(static_cast<int32_t>(FILE_MAGIC));
	out.writeVarInt(static_cast<int32_t>(_byPos.size()));
	for (const auto& [key, record] : _byPos) {
		out.writeLong(key);
		out.writeUByte(static_cast<uint8_t>(record.type));
		out.writeVarInt(record.freeTickets);
	}
	return std::move(out.getData());
}

void PoiManager::load() {
	if (!std::filesystem::exists(_file)) return;
	try {
		std::ifstream		 in(_file, std::ios::binary);
		std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		Buffer				 data(bytes);
		if (static_cast<uint32_t>(data.readInt()) != FILE_MAGIC) throw std::runtime_error("not a POI file");
		int count = data.readVarInt();
		for (int i = 0; i < count; i++) {
			int64_t key	   = data.readLong();
			int		type   = data.readUByte();
			int		free   = data.readVarInt();
			if (type >= static_cast<int>(Type::Count)) continue;
			BlockPos pos = BlockPos::fromLong(key);
			_byPos[key]	 = {pos, static_cast<Type>(type), free};
			_byChunk[chunkKeyOf(pos)].push_back(key);
		}
	} catch (const std::exception& e) {
		g_logger->logGameInfo(WARN, "Unreadable " + _file.string() + " (" + e.what() + "): points of interest found again as chunks load", "World");
		_byPos.clear();
		_byChunk.clear();
	}
}
