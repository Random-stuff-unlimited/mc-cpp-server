#ifndef ANVIL_IMPORTER_HPP
#define ANVIL_IMPORTER_HPP

#include "world/ChunkStorage.hpp"

#include <atomic>
#include <filesystem>
#include <memory>
#include <optional>

class GameData;

// Reads chunks from a vanilla world (<world>/region/*.mca) so an existing map can be dropped into the server:
// each chunk is converted the first time it is loaded, then saved in the server's own format.
class AnvilImporter {
  public:
	struct Spawn {
		int x, y, z;
	};

	AnvilImporter(const std::filesystem::path& worldDirectory, const GameData& gameData, const ChunkStorage::Layout& layout);

	bool available() const { return _available; }

	// nullptr if the chunk doesn't exist or isn't fully generated
	std::unique_ptr<Chunk> load(int x, int z);

	// Spawn point from level.dat, if there is one
	static std::optional<Spawn> readSpawn(const std::filesystem::path& worldDirectory);

  private:
	std::filesystem::path _regionDirectory;
	const GameData&		  _gameData;
	ChunkStorage::Layout  _layout;
	bool				  _available;
	std::atomic<size_t>	  _unknownBlocks{0};
};

#endif
