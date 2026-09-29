#include "world/PlayerDataStorage.hpp"

#include "data/GameData.hpp"
#include "lib/compression.hpp"
#include "lib/nbtParser.hpp"
#include "lib/nbtWriter.hpp"
#include "logger.hpp"
#include "player.hpp"
#include "world/item/ItemNbt.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace {
	using nbt::Tag;
	using nbt::TagCompound;
	using nbt::TagList;
	using Compound = std::shared_ptr<TagCompound>;
	using List	   = std::shared_ptr<TagList>;

	constexpr size_t MAX_FILE_SIZE = 64 * 1024 * 1024; // Uncompressed

	// Values of what the server doesn't simulate yet. Once it does, the value is saved and read like Health
	constexpr float	  MAX_HEALTH	   = 20;  // LivingEntity.getMaxHealth
	constexpr int	  ENDER_CHEST_SIZE = 27;

	// EquipmentSlot names (EntityEquipment.CODEC) and their slot in the inventory window
	struct EquipmentSlot {
		const char* name;
		int			window;
	};
	constexpr std::array<EquipmentSlot, 5> EQUIPMENT = {{{"feet", 8}, {"legs", 7}, {"chest", 6}, {"head", 5}, {"offhand", PlayerInventory::OFFHAND}}};

	// RecipeBookSettings.TypeSettings keys, by recipe book type (crafting, furnace, blast furnace, smoker)
	constexpr std::array<std::pair<const char*, const char*>, 4> RECIPE_BOOK_KEYS = {{{"isGuiOpen", "isFilteringCraftable"},
																					  {"isFurnaceGuiOpen", "isFurnaceFilteringCraftable"},
																					  {"isBlastingFurnaceGuiOpen", "isBlastingFurnaceFilteringCraftable"},
																					  {"isSmokerGuiOpen", "isSmokerFilteringCraftable"}}};

	Tag compoundTag(TagCompound compound) { return Tag(std::make_shared<TagCompound>(std::move(compound))); }
	Tag listTag(TagList list) { return Tag(std::make_shared<TagList>(std::move(list))); }

	// ----- Reading, with the codecs' defaults when a value is missing or of the wrong type -----

	std::optional<double> number(const TagCompound& data, const std::string& key) {
		if (!data.contains(key)) return std::nullopt;
		return std::visit(
				[](const auto& value) -> std::optional<double> {
					using T = std::decay_t<decltype(value)>;
					if constexpr (std::is_arithmetic_v<T>) {
						return static_cast<double>(value);
					} else {
						return std::nullopt;
					}
				},
				data.at(key).data);
	}
	int	   intOr(const TagCompound& data, const std::string& key, int fallback) { return static_cast<int>(number(data, key).value_or(fallback)); }
	float  floatOr(const TagCompound& data, const std::string& key, float fallback) { return static_cast<float>(number(data, key).value_or(fallback)); }
	double doubleOr(const TagCompound& data, const std::string& key, double fallback) { return number(data, key).value_or(fallback); }
	bool   boolOr(const TagCompound& data, const std::string& key, bool fallback) { return number(data, key).value_or(fallback) != 0; }
	std::string stringOr(const TagCompound& data, const std::string& key, const std::string& fallback) {
		if (!data.contains(key)) return fallback;
		const auto* value = std::get_if<nbt::TagString>(&data.at(key).data);
		return value ? *value : fallback;
	}
	const TagCompound* compound(const TagCompound& data, const std::string& key) {
		if (!data.contains(key)) return nullptr;
		const auto* value = std::get_if<Compound>(&data.at(key).data);
		return value ? value->get() : nullptr;
	}
	const TagList* list(const TagCompound& data, const std::string& key) {
		if (!data.contains(key)) return nullptr;
		const auto* value = std::get_if<List>(&data.at(key).data);
		return value ? value->get() : nullptr;
	}
	// Vec3.CODEC / Vec2.CODEC: a list of numbers
	std::vector<double> numbers(const TagCompound& data, const std::string& key) {
		std::vector<double> values;
		if (const TagList* elements = list(data, key)) {
			for (const Tag& element : elements->data) {
				TagCompound wrapper;
				wrapper[""] = element;
				if (std::optional<double> value = number(wrapper, "")) values.push_back(*value);
			}
		}
		return values;
	}
	void putIfMissing(TagCompound& data, const std::string& key, Tag value) {
		if (!data.contains(key)) data[key] = std::move(value);
	}

	// ----- Items -----

	// ItemStackWithSlot.CODEC: the stack's fields and "Slot" (unsigned byte)
	TagList saveSlots(const ItemStack* items, int size, const GameData& gameData) {
		TagList slots;
		for (int i = 0; i < size; i++) {
			const ItemStack& stack = items[i];
			if (stack.isEmpty()) continue;
			TagCompound entry = ItemNbt::save(stack, gameData);
			entry["Slot"]	  = nbt::TagByte(static_cast<int8_t>(i));
			slots.push_back(compoundTag(std::move(entry)));
		}
		return slots;
	}

	// ValueInput.listOrEmpty(ItemStackWithSlot.CODEC): the entries that are items with a slot
	template <typename Place> void loadSlots(const TagCompound& data, const std::string& key, const GameData& gameData, bool trustRaw, Place place) {
		const TagList* entries = list(data, key);
		if (!entries) return;
		for (const Tag& element : entries->data) {
			const auto* entry = std::get_if<Compound>(&element.data);
			if (!entry || !*entry) continue;
			int		  slot	= intOr(**entry, "Slot", 0) & 0xFF;
			ItemStack stack = ItemNbt::load(**entry, gameData, trustRaw);
			if (!stack.isEmpty()) place(slot, std::move(stack));
		}
	}

	// GameType.LEGACY_ID_CODEC: -1 = none, an unknown id is survival (GameType.byId)
	int gameTypeOf(const TagCompound& data, const std::string& key) {
		std::optional<double> id = number(data, key);
		if (!id || *id == -1) return -1;
		return *id >= 0 && *id <= 3 ? static_cast<int>(*id) : 0;
	}

	std::string uuidFileName(const UUID& uuid) { return uuid.toString(); }
} // namespace

// ===================== PlayerData =====================

namespace PlayerData {
	nbt::TagCompound save(const Player& player, const GameData& gameData, const std::string& dimension) {
		// What the loaded file had and the server doesn't know about stays as it was
		TagCompound data = player.savedData() ? *player.savedData() : TagCompound();
		const CombatState& combat = player.combat();

		// Entity.saveWithoutId
		data["Pos"]			  = listTag(TagList(std::vector<double>{player.getX(), player.getY(), player.getZ()}));
		data["Motion"]		  = listTag(TagList(std::vector<double>{0.0, 0.0, 0.0})); // Movements are the client's
		data["Rotation"]	  = listTag(TagList(std::vector<float>{player.getYaw(), player.getPitch()}));
		data["fall_distance"] = nbt::TagDouble(combat.fallDistance);
		data["Fire"]		  = nbt::TagShort(static_cast<int16_t>(std::clamp(player.survival().remainingFireTicks, -32768, 32767)));
		data["Air"]			  = nbt::TagShort(static_cast<int16_t>(player.getAirSupply()));
		data["OnGround"]	  = nbt::TagByte(player.isOnGround());
		putIfMissing(data, "Invulnerable", nbt::TagByte(0));
		data["PortalCooldown"] = nbt::TagInt(player.portal.cooldown);
		const UUID& uuid = player.getUUID();
		data["UUID"]	 = nbt::TagIntArray{static_cast<int32_t>(uuid.getMostSigBits() >> 32), static_cast<int32_t>(uuid.getMostSigBits()),
										static_cast<int32_t>(uuid.getLeastSigBits() >> 32), static_cast<int32_t>(uuid.getLeastSigBits())};

		// LivingEntity.addAdditionalSaveData
		data["Health"]			= nbt::TagFloat(combat.health);
		data["HurtTime"]		= nbt::TagShort(0);
		data["HurtByTimestamp"] = nbt::TagInt(0);
		data["DeathTime"]		= nbt::TagShort(0);
		putIfMissing(data, "AbsorptionAmount", nbt::TagFloat(0));
		data["FallFlying"] = nbt::TagByte(0);
		// EntityEquipment.CODEC: the non-empty slots by name. PlayerEquipment.isEmpty counts the selected item
		TagCompound equipment;
		for (const EquipmentSlot& slot : EQUIPMENT) {
			const ItemStack& stack = player.inventory().get(slot.window);
			if (!stack.isEmpty()) equipment[slot.name] = compoundTag(ItemNbt::save(stack, gameData));
		}
		data.data.erase("equipment");
		if (equipment.size() > 0 || !player.getStackInHand(0).isEmpty()) data["equipment"] = compoundTag(std::move(equipment));

		// Player.addAdditionalSaveData
		data["DataVersion"] = nbt::TagInt(gameData.getDataVersion()); // NbtUtils.addCurrentDataVersion
		std::array<ItemStack, 36> items;							  // Inventory.items: 0-8 hotbar, 9-35 main
		for (int i = 0; i < 36; i++) items[i] = player.inventory().get(PlayerInventory::windowSlot(i));
		data["Inventory"]		 = listTag(saveSlots(items.data(), 36, gameData));
		data["SelectedItemSlot"] = nbt::TagInt(player.getSelectedSlot());
		data["SleepTimer"]		 = nbt::TagShort(0);
data["XpP"]		 = nbt::TagFloat(player.getXpProgress());
	data["XpLevel"]	 = nbt::TagInt(player.getXpLevel());
	data["XpTotal"]	 = nbt::TagInt(player.getXpTotal());
	data["XpSeed"]	 = nbt::TagInt(player.getXpSeed());
		putIfMissing(data, "Score", nbt::TagInt(0));
		// FoodData.addAdditionalSaveData
		const FoodData& food		= player.foodData();
		data["foodLevel"]			= nbt::TagInt(food.getFoodLevel());
		data["foodTickTimer"]		= nbt::TagInt(food.getTickTimer());
		data["foodSaturationLevel"] = nbt::TagFloat(food.getSaturationLevel());
		data["foodExhaustionLevel"] = nbt::TagFloat(food.getExhaustionLevel());
		// Abilities.Packed, as GameType.updatePlayerAbilities sets them
		GameMode	mode = player.getGameMode();
		TagCompound abilities;
		abilities["invulnerable"] = nbt::TagByte(mode == GameMode::Creative || mode == GameMode::Spectator);
		abilities["flying"]		  = nbt::TagByte(mode == GameMode::Spectator);
		abilities["mayfly"]		  = nbt::TagByte(mode == GameMode::Creative || mode == GameMode::Spectator);
		abilities["instabuild"]	  = nbt::TagByte(mode == GameMode::Creative);
		abilities["mayBuild"]	  = nbt::TagByte(mode == GameMode::Survival || mode == GameMode::Creative);
		abilities["flySpeed"]	  = nbt::TagFloat(0.05F);
		abilities["walkSpeed"]	  = nbt::TagFloat(0.1F);
		data["abilities"]		  = compoundTag(std::move(abilities));
		data["EnderItems"] = listTag(saveSlots(player.enderChest().data(), ENDER_CHEST_SIZE, gameData));
		data.data.erase("LastDeathLocation");
		if (combat.hasDeathLocation) { // GlobalPos.CODEC
			TagCompound location;
			location["dimension"]	  = nbt::TagString(dimension);
			location["pos"]			  = nbt::TagIntArray{combat.deathX, combat.deathY, combat.deathZ};
			data["LastDeathLocation"] = compoundTag(std::move(location));
		}
		putIfMissing(data, "ignore_fall_damage_from_current_explosion", nbt::TagByte(0));
		putIfMissing(data, "current_impulse_context_reset_grace_time", nbt::TagInt(0));

		// ServerPlayer.addAdditionalSaveData
		data["playerGameType"] = nbt::TagInt(static_cast<int>(mode));
		data.data.erase("previousPlayerGameType");
		if (player.getPreviousGameMode() >= 0) data["previousPlayerGameType"] = nbt::TagInt(player.getPreviousGameMode());
		data["seenCredits"] = nbt::TagByte(player.seenCredits());
		// ServerRecipeBook.Packed: every recipe is known, the lists of the loaded file are kept as they were
		TagCompound recipeBook;
		if (const TagCompound* saved = compound(data, "recipeBook")) recipeBook = *saved;
		putIfMissing(recipeBook, "recipes", listTag({}));
		putIfMissing(recipeBook, "toBeDisplayed", listTag({}));
		for (size_t type = 0; type < RECIPE_BOOK_KEYS.size(); type++) { // optionalFieldOf(key, false): only when true
			auto [open, filtering] = RECIPE_BOOK_KEYS[type];
			recipeBook.data.erase(open);
			recipeBook.data.erase(filtering);
			if (player.recipeBookSettings()[type * 2]) recipeBook[open] = nbt::TagByte(1);
			if (player.recipeBookSettings()[type * 2 + 1]) recipeBook[filtering] = nbt::TagByte(1);
		}
		data["recipeBook"] = compoundTag(std::move(recipeBook));
		data["Dimension"]  = nbt::TagString(dimension);
		// ServerPlayer.addAdditionalSaveData: its respawn point (SpawnX/Y/Z, SpawnDimension, SpawnForced)
		for (const char* key : {"SpawnX", "SpawnY", "SpawnZ", "SpawnDimension", "SpawnForced", "spawn"}) data.data.erase(key);
		if (player.spawn().valid) {
			const PlayerSpawn& spawn = player.spawn();
			data["SpawnX"]			= nbt::TagInt(spawn.x);
			data["SpawnY"]			= nbt::TagInt(spawn.y);
			data["SpawnZ"]			= nbt::TagInt(spawn.z);
			data["SpawnDimension"]	= nbt::TagString(spawn.dimension);
			data["SpawnForced"]		= nbt::TagByte(spawn.forced);
		}
		putIfMissing(data, "spawn_extra_particles_on_fall", nbt::TagByte(0));
		return data;
	}

	std::string dimension(const nbt::TagCompound& data) { return stringOr(data, "Dimension", ""); }

	void load(Player& player, const nbt::TagCompound& data, const GameData& gameData, GameMode defaultGameMode) {
		player.setSavedData(std::make_shared<const TagCompound>(data));
		// Our raw component patches only mean something with this version's ids
		bool		 trustRaw = intOr(data, "DataVersion", -1) == gameData.getDataVersion();
		CombatState& combat	  = player.combat();

		// Entity.load: the position clamped to the world border's limits, rotation and pitch checked
		std::vector<double> pos = numbers(data, "Pos");
		if (pos.size() == 3 && std::isfinite(pos[0]) && std::isfinite(pos[1]) && std::isfinite(pos[2])) {
			player.setPosition(std::clamp(pos[0], -3.0000512E7, 3.0000512E7), std::clamp(pos[1], -2.0E7, 2.0E7), std::clamp(pos[2], -3.0000512E7, 3.0000512E7));
		}
		std::vector<double> rotation = numbers(data, "Rotation");
		if (rotation.size() == 2 && std::isfinite(rotation[0]) && std::isfinite(rotation[1])) {
			player.setRotation(static_cast<float>(rotation[0]), std::clamp(std::fmod(static_cast<float>(rotation[1]), 360.0F), -90.0F, 90.0F));
		}
		combat.fallDistance = doubleOr(data, "fall_distance", doubleOr(data, "FallDistance", 0)); // Float before 1.21.5
		player.setOnGround(boolOr(data, "OnGround", false));
		player.survival().remainingFireTicks = intOr(data, "Fire", -20);
		player.portal.cooldown				 = intOr(data, "PortalCooldown", 0);
		player.setSeenCredits(boolOr(data, "seenCredits", false));

		// LivingEntity.readAdditionalSaveData
		combat.health = std::min(floatOr(data, "Health", MAX_HEALTH), MAX_HEALTH);
		player.setAirSupply(intOr(data, "Air", 300)); // Entity.load
		combat.dead	  = combat.health <= 0;
		if (combat.dead) combat.health = 0;
		for (int slot = 0; slot < PlayerInventory::SIZE; slot++) player.inventory().set(slot, ItemStack());
		if (const TagCompound* equipment = compound(data, "equipment")) {
			for (const EquipmentSlot& slot : EQUIPMENT) {
				if (const TagCompound* item = compound(*equipment, slot.name)) player.inventory().set(slot.window, ItemNbt::load(*item, gameData, trustRaw));
			}
		}

		// Player.readAdditionalSaveData
		loadSlots(data, "Inventory", gameData, trustRaw, [&player](int slot, ItemStack stack) {
			if (slot < 36) {
				player.inventory().set(PlayerInventory::windowSlot(slot), std::move(stack));
			} else if (slot >= 100 && slot <= 103) { // Armor before 1.21.5 (feet to head)
				player.inventory().set(8 - (slot - 100), std::move(stack));
			} else if (slot == 150) { // The offhand before 1.21.5 (-106)
				player.inventory().set(PlayerInventory::OFFHAND, std::move(stack));
			}
		});
		int selected = intOr(data, "SelectedItemSlot", 0);
		player.setSelectedSlot(selected >= 0 && selected < 9 ? selected : 0);
		// FoodData.readAdditionalSaveData
		FoodData& food = player.foodData();
		food.setFoodLevel(intOr(data, "foodLevel", 20));
		food.setTickTimer(intOr(data, "foodTickTimer", 0));
		food.setSaturation(floatOr(data, "foodSaturationLevel", 5));
		food.setExhaustion(floatOr(data, "foodExhaustionLevel", 0));
		for (ItemStack& stack : player.enderChest()) stack = ItemStack();
		loadSlots(data, "EnderItems", gameData, trustRaw, [&player](int slot, ItemStack stack) {
			if (slot < ENDER_CHEST_SIZE) player.enderChest()[static_cast<size_t>(slot)] = std::move(stack);
		});
		combat.hasDeathLocation = false;
		if (const TagCompound* location = compound(data, "LastDeathLocation")) {
			if (location->contains("pos")) {
				if (const auto* position = std::get_if<nbt::TagIntArray>(&location->at("pos").data); position && position->size() == 3) {
					combat.hasDeathLocation = true;
					combat.deathX			= (*position)[0];
					combat.deathY			= (*position)[1];
					combat.deathZ			= (*position)[2];
				}
			}
		}

		// ServerPlayer.readAdditionalSaveData
		player.setXpTotal(intOr(data, "XpTotal", 0));
		player.setXpLevel(intOr(data, "XpLevel", 0));
		player.setXpProgress(floatOr(data, "XpP", 0));
		player.setXpSeed(intOr(data, "XpSeed", 0));

		// ServerPlayer.readAdditionalSaveData
		int gameType = gameTypeOf(data, "playerGameType");
		player.setGameMode(gameType >= 0 ? static_cast<GameMode>(gameType) : defaultGameMode); // calculateGameModeForNewPlayer
		player.setPreviousGameMode(gameTypeOf(data, "previousPlayerGameType"));
		std::array<bool, 8>& settings = player.recipeBookSettings();
		settings.fill(false);
		if (const TagCompound* recipeBook = compound(data, "recipeBook")) {
			for (size_t type = 0; type < RECIPE_BOOK_KEYS.size(); type++) {
				settings[type * 2]	   = boolOr(*recipeBook, RECIPE_BOOK_KEYS[type].first, false);
				settings[type * 2 + 1] = boolOr(*recipeBook, RECIPE_BOOK_KEYS[type].second, false);
			}
		}

		// ServerPlayer.readAdditionalSaveData: its respawn point, the legacy SpawnX/Y/Z or the 1.21.9+ "spawn" compound
		PlayerSpawn& spawn = player.spawn();
		spawn.valid		   = false;
		if (data.contains("SpawnX")) {
			spawn.valid	  = true;
			spawn.x		  = intOr(data, "SpawnX", 0);
			spawn.y		  = intOr(data, "SpawnY", 0);
			spawn.z		  = intOr(data, "SpawnZ", 0);
			spawn.dimension = stringOr(data, "SpawnDimension", "minecraft:overworld");
			spawn.forced  = boolOr(data, "SpawnForced", false);
		} else if (const TagCompound* saved = compound(data, "spawn")) {
			if (saved->contains("pos")) {
				if (const auto* position = std::get_if<nbt::TagIntArray>(&saved->at("pos").data); position && position->size() == 3) {
					spawn.valid	  = true;
					spawn.x		  = (*position)[0];
					spawn.y		  = (*position)[1];
					spawn.z		  = (*position)[2];
					spawn.dimension = stringOr(*saved, "dimension", "minecraft:overworld");
					spawn.forced  = boolOr(*saved, "forced", false);
				}
			}
		}
	}
} // namespace PlayerData

// ===================== PlayerDataStorage =====================

PlayerDataStorage::PlayerDataStorage(const std::filesystem::path& worldDirectory, Submit submit)
	: _directory(worldDirectory / "playerdata"), _submit(std::move(submit)) {}

std::optional<nbt::TagCompound> PlayerDataStorage::load(const UUID& uuid) {
	std::string name = uuidFileName(uuid);
	{
		std::lock_guard<std::mutex> lock(_mutex);
		auto						it = _pending.find(name);
		if (it != _pending.end()) return *it->second.data;
	}
	std::lock_guard<std::mutex> lock(_fileMutex);
	std::filesystem::path		file = _directory / (name + ".dat");
	try {
		if (std::optional<nbt::TagCompound> data = read(file)) return data;
	} catch (const std::exception& e) {
		g_logger->logGameInfo(WARN, "Failed to load player data for " + name + ": " + e.what(), "PlayerData");
		// PlayerDataStorage.backup: the unreadable file is kept aside
		char	date[32];
		std::time_t now = std::time(nullptr);
		std::strftime(date, sizeof(date), "%Y-%m-%d_%H.%M.%S", std::localtime(&now));
		std::error_code error;
		std::filesystem::copy_file(file, _directory / (name + "_corrupted_" + date + ".dat"), std::filesystem::copy_options::overwrite_existing, error);
	}
	try {
		return read(_directory / (name + ".dat_old"));
	} catch (const std::exception& e) {
		g_logger->logGameInfo(WARN, "Failed to load player data for " + name + ": " + e.what(), "PlayerData");
		return std::nullopt;
	}
}

std::optional<nbt::TagCompound> PlayerDataStorage::read(const std::filesystem::path& file) {
	if (!std::filesystem::is_regular_file(file)) return std::nullopt;
	std::ifstream		 in(file, std::ios::binary);
	std::vector<uint8_t> compressed((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	if (!in.good() && !in.eof()) throw std::runtime_error("read error");
	// NbtIo.readCompressed: gzip
	std::vector<uint8_t> raw = compression::inflateUnknownSize(compressed.data(), compressed.size(), true, MAX_FILE_SIZE);
	return nbt::Parser().parse(raw).getRoot();
}

void PlayerDataStorage::save(const UUID& uuid, nbt::TagCompound data) {
	std::string name = uuidFileName(uuid);
	uint64_t	sequence;
	{
		std::lock_guard<std::mutex> lock(_mutex);
		sequence	  = ++_nextSequence;
		_pending[name] = {std::make_shared<const nbt::TagCompound>(std::move(data)), sequence};
	}
	if (_submit) {
		_submit([this, name, sequence] { write(name, sequence); });
	} else {
		write(name, sequence);
	}
}

// PlayerDataStorage.save: to a temporary file, then Util.safeReplaceFile
void PlayerDataStorage::write(const std::string& uuid, uint64_t sequence) {
	std::lock_guard<std::mutex>				fileLock(_fileMutex);
	std::shared_ptr<const nbt::TagCompound> data;
	{
		std::lock_guard<std::mutex> lock(_mutex);
		auto						it = _pending.find(uuid);
		if (it == _pending.end() || it->second.sequence != sequence) return; // A later save of this player writes it
		data = it->second.data;
	}
	try {
		std::vector<uint8_t> raw		= nbt::Writer::write(*data);
		std::vector<uint8_t> compressed = compression::gzipCompress(raw.data(), raw.size(), 6);
		std::filesystem::create_directories(_directory);
		std::filesystem::path temporary = _directory / (uuid + "-" + std::to_string(sequence) + ".dat");
		{
			std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
			out.write(reinterpret_cast<const char*>(compressed.data()), static_cast<std::streamsize>(compressed.size()));
			out.close();
			if (!out) throw std::runtime_error("can't write " + temporary.string());
		}
		std::filesystem::path file = _directory / (uuid + ".dat");
		if (std::filesystem::exists(file)) std::filesystem::rename(file, _directory / (uuid + ".dat_old"));
		std::filesystem::rename(temporary, file);
	} catch (const std::exception& e) {
		g_logger->logGameInfo(WARN, "Failed to save player data for " + uuid + ": " + e.what(), "PlayerData");
		return; // Kept pending: a reconnection still gets it
	}
	std::lock_guard<std::mutex> lock(_mutex);
	auto						it = _pending.find(uuid);
	if (it != _pending.end() && it->second.sequence == sequence) _pending.erase(it);
}
