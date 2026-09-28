#ifndef PLAYER_DATA_STORAGE_HPP
#define PLAYER_DATA_STORAGE_HPP

#include "lib/nbt.hpp"
#include "lib/UUID.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

class GameData;
class Player;
enum class GameMode : uint8_t;

// A player's saved state, in vanilla's format (ServerPlayer.addAdditionalSaveData and readAdditionalSaveData, with
// Player's, LivingEntity's and Entity's). Game thread
namespace PlayerData {
	// Entity.saveWithoutId for a player. dimension: the one it is in ("minecraft:overworld")
	nbt::TagCompound save(const Player& player, const GameData& gameData, const std::string& dimension);
	// Entity.load: sets the player from its data (keeping the data for the next save). defaultGameMode is the
	// server's, for data without a game mode (ServerPlayer.calculateGameModeForNewPlayer)
	void load(Player& player, const nbt::TagCompound& data, const GameData& gameData, GameMode defaultGameMode);
	// The "Dimension" of the data, empty if none (ServerPlayer.SavedPosition)
	std::string dimension(const nbt::TagCompound& data);
} // namespace PlayerData

// PlayerDataStorage: <world>/playerdata/<uuid>.dat, gzipped NBT. A save goes to a temporary file that then
// replaces the previous one, kept as <uuid>.dat_old (Util.safeReplaceFile); a load falls back to it.
//
// Files are written on the I/O threads (submit): only the last save of a player is written, and a load right
// after a save (a quick reconnection) gets the data not written yet
class PlayerDataStorage {
  public:
	using Submit = std::function<void(std::function<void()>)>;

	// submit runs a job on an I/O thread; without it files are written at once
	PlayerDataStorage(const std::filesystem::path& worldDirectory, Submit submit = nullptr);

	// PlayerDataStorage.load: nullopt for a player that never played here (or whose files are unreadable)
	std::optional<nbt::TagCompound> load(const UUID& uuid);
	void							save(const UUID& uuid, nbt::TagCompound data);

	const std::filesystem::path& directory() const { return _directory; }

  private:
	struct Pending {
		std::shared_ptr<const nbt::TagCompound> data;
		uint64_t								sequence;
	};

	std::filesystem::path					 _directory;
	Submit									 _submit;
	std::mutex								 _mutex;	 // _pending
	std::mutex								 _fileMutex; // The files: one write at a time
	std::unordered_map<std::string, Pending> _pending;	 // Saves not written yet, by UUID
	uint64_t								 _nextSequence = 0;

	void						   write(const std::string& uuid, uint64_t sequence);
	std::optional<nbt::TagCompound> read(const std::filesystem::path& file);
};

#endif
