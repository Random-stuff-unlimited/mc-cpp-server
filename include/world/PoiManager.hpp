#ifndef POI_MANAGER_HPP
#define POI_MANAGER_HPP

#include "world/BlockPos.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class Chunk;
class GameData;

// Points of interest (vanilla's PoiManager and PoiTypes): the blocks villagers, bees and portals look for — nether
// portals, beds, job sites, bells, hives, lodestones, lightning rods. Every one of them in the dimension is indexed
// (loaded chunks or not), so a portal can find the closest one 128 blocks away without loading chunks. Kept up to date
// as blocks change, checked against the blocks when a chunk loads (the data may be older than the blocks), and saved
// in <world>/poi.dat. Game thread only.
class PoiManager {
  public:
	// PoiTypes, in registry order
	enum class Type : uint8_t {
		Armorer,
		Butcher,
		Cartographer,
		Cleric,
		Farmer,
		Fisherman,
		Fletcher,
		Leatherworker,
		Librarian,
		Mason,
		Shepherd,
		Toolsmith,
		Weaponsmith,
		Home,
		Meeting,
		Beehive,
		BeeNest,
		NetherPortal,
		Lodestone,
		LightningRod,
		TestInstance,
		Count
	};
	// PoiManager.Occupancy
	enum class Occupancy { HasSpace, IsOccupied, Any };
	struct Record {
		BlockPos pos;
		Type	 type;
		int		 freeTickets;
	};

	PoiManager(const GameData& gameData, std::filesystem::path file);

	// PoiTypes.forState: the type of a block state, if it is a point of interest
	std::optional<Type> typeOf(int state) const {
		int8_t type = _typeByState[state];
		return type < 0 ? std::nullopt : std::optional<Type>(static_cast<Type>(type));
	}
	static int maxTickets(Type type);
	static int validRange(Type type);

	// A block changed (ServerLevel.onBlockStateChange): the record goes, the new one comes
	void onBlockChanged(const BlockPos& pos, int oldState, int newState);
	// A chunk joined the level: its records are checked against its blocks (PoiManager.checkConsistencyWithBlocks)
	void onChunkLoaded(const Chunk& chunk);
	// PoiManager.getInSquare: the records of a type within `radius` blocks on x and z (any y)
	std::vector<const Record*> getInSquare(Type type, const BlockPos& center, int radius, Occupancy occupancy) const;
	// PoiManager.getInRange: the same within a sphere (distance <= radius)
	std::vector<const Record*> getInRange(Type type, const BlockPos& center, int radius, Occupancy occupancy) const;
	const Record* get(const BlockPos& pos) const;
	// PoiManager.take / release: a villager claims a job site or bed ticket
	bool take(const BlockPos& pos);
	bool release(const BlockPos& pos);

	// The index to disk (poi.dat), written on an I/O thread from the returned bytes
	std::vector<uint8_t> encode() const;
	const std::filesystem::path& file() const { return _file; }
	bool						 isDirty() const { return _dirty; }
	void						 setDirty(bool dirty) { _dirty = dirty; }
	size_t						 size() const { return _byPos.size(); }

  private:
	std::filesystem::path								_file;
	std::vector<int8_t>									_typeByState;
	std::unordered_map<int64_t, Record>					_byPos;	  // BlockPos.asLong -> record
	std::unordered_map<int64_t, std::vector<int64_t>>	_byChunk; // Chunk key -> positions
	bool												_dirty = false;

	void add(const BlockPos& pos, Type type);
	void remove(const BlockPos& pos);
	void load();
	static bool matches(const Record& record, Occupancy occupancy);
};

#endif
