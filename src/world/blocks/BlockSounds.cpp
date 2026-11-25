#include "world/blocks/BlockSounds.hpp"

#include "data/GameData.hpp"

#include <unordered_map>
#include <utility>
#include <vector>

namespace {
	bool has(const std::string& name, const char* part) { return name.find(part) != std::string::npos; }

	// The sound type of a block name (minecraft: prefix removed): its own when the game has one by that name, else
	// by material
	std::string typeOf(const std::string& name, const GameData& gameData) {
		// Blocks with their own sound type, named after them
		if (gameData.getStaticId("minecraft:sound_event", "minecraft:block." + name + ".step") >= 0) return name;
		static const std::vector<std::pair<const char*, const char*>> BY_PART = {
			{"bamboo_planks", "bamboo_wood"}, {"bamboo_mosaic", "bamboo_wood"}, {"cherry_", "cherry_wood"},	   {"crimson_", "nether_wood"},
			{"warped_", "nether_wood"},	  {"hanging_sign", "hanging_sign"},	  {"_planks", "wood"},			   {"_log", "wood"},
			{"_wood", "wood"},			  {"_door", "wood"},				  {"_trapdoor", "wood"},		   {"_fence", "wood"},
			{"_sign", "wood"},			  {"bookshelf", "wood"},			  {"crafting_table", "wood"},	   {"chest", "wood"},
			{"barrel", "wood"},			  {"note_block", "wood"},			  {"jukebox", "wood"},			   {"_leaves", "grass"},
			{"grass", "grass"},			  {"fern", "grass"},				  {"sapling", "grass"},			   {"flower", "grass"},
			{"tulip", "grass"},			  {"dandelion", "grass"},			  {"poppy", "grass"},			   {"_mushroom", "grass"},
			{"dirt", "grass"},			  {"farmland", "grass"},			  {"podzol", "grass"},			   {"mycelium", "grass"},
			{"hay_block", "grass"},		  {"wheat", "grass"},				  {"carrots", "grass"},			   {"potatoes", "grass"},
			{"beetroots", "grass"},		  {"sugar_cane", "grass"},			  {"kelp", "grass"},			   {"seagrass", "grass"},
			{"sand", "sand"},			  {"gravel", "gravel"},				  {"clay", "gravel"},			   {"_wool", "wool"},
			{"_carpet", "wool"},		  {"_bed", "wool"},					  {"glass", "glass"},			   {"ice", "glass"},
			{"beacon", "glass"},		  {"sea_lantern", "glass"},			  {"snow", "snow"},				   {"iron_", "metal"},
			{"gold_block", "metal"},	  {"diamond_block", "metal"},		  {"emerald_block", "metal"},	   {"cauldron", "metal"},
			{"hopper", "metal"},		  {"rail", "metal"},				  {"copper", "copper"},			   {"netherrack", "netherrack"},
			{"nether_brick", "nether_bricks"}, {"deepslate", "deepslate"},	  {"tuff", "tuff"},				   {"basalt", "basalt"},
			{"blackstone", "stone"},	  {"vine", "vine"},					  {"scaffolding", "scaffolding"},  {"slime", "slime_block"},
			{"honey", "honey_block"},	  {"cobweb", "cobweb"},				  {"cactus", "wool"},			   {"cake", "wool"},
			{"sponge", "sponge"},		  {"moss", "moss"},					  {"mud", "mud"},				   {"coral", "coral_block"},
		};
		for (const auto& [part, type] : BY_PART) {
			if (has(name, part)) return type;
		}
		return "stone";
	}
} // namespace

namespace BlockSounds {
	std::string sound(const GameData& gameData, int block, const char* kind) {
		static std::unordered_map<int, std::string> cache;
		auto										 it = cache.find(block);
		if (it == cache.end()) {
			std::string name = gameData.getStaticName("minecraft:block", block);
			if (name.rfind("minecraft:", 0) == 0) name = name.substr(10);
			it = cache.emplace(block, typeOf(name, gameData)).first;
		}
		std::string sound = "minecraft:block." + it->second + "." + kind;
		if (gameData.getStaticId("minecraft:sound_event", sound) < 0) sound = std::string("minecraft:block.stone.") + kind;
		return sound;
	}
} // namespace BlockSounds
