#include "world/entity/Variants.hpp"

#include "data/GameData.hpp"
#include "lib/json.hpp"
#include "world/Level.hpp"

#include <algorithm>
#include <fstream>
#include <unordered_map>
#include <vector>

namespace Variants {
	namespace {
		struct Selector {
			nlohmann::json condition; // Null: always true
			int			   priority;
		};
		struct Variant {
			std::string			  name;
			std::vector<Selector> selectors;
		};
		using Registry = std::vector<Variant>; // By id

		// entity_variants.json, loaded once per game data
		const Registry* registry(Level& level, const std::string& name) {
			static const GameData*							 loadedFor = nullptr;
			static std::unordered_map<std::string, Registry> registries;
			const GameData&									 data = level.gameData();
			if (loadedFor != &data) {
				loadedFor = &data;
				registries.clear();
				std::ifstream file(data.getDirectory() / "entity_variants.json");
				if (file) {
					nlohmann::json json = nlohmann::json::parse(file);
					for (const auto& [key, entries] : json.items()) {
						Registry& reg = registries[key];
						for (const auto& entry : entries) {
							Variant variant{entry.at(0).get<std::string>(), {}};
							for (const auto& selector : entry.at(1)) {
								variant.selectors.push_back({selector.value("condition", nlohmann::json()), selector.value("priority", 0)});
							}
							reg.push_back(std::move(variant));
						}
					}
				}
			}
			auto it = registries.find(name);
			return it == registries.end() ? nullptr : &it->second;
		}

		// SpawnCondition.test (biome, structure, moon brightness)
		bool test(Level& level, const nlohmann::json& condition, const BlockPos& pos) {
			if (condition.is_null()) return true;
			std::string type = condition.value("type", "");
			if (type == "minecraft:biome") {
				const GameData&		  data	= level.gameData();
				int					  biome = level.getBiomeId(pos);
				auto				  holds = [&](const std::string& entry) {
					   if (entry.rfind('#', 0) == 0) return data.isInTag("minecraft:worldgen/biome", entry.substr(1), biome);
					   return data.getSyncedId("minecraft:worldgen/biome", entry) == biome;
				};
				const nlohmann::json& biomes = condition.at("biomes");
				if (biomes.is_string()) return holds(biomes.get<std::string>());
				return std::any_of(biomes.begin(), biomes.end(), [&](const nlohmann::json& b) { return holds(b.get<std::string>()); });
			}
			if (type == "minecraft:moon_brightness") {
				float				  brightness = level.getMoonBrightness();
				const nlohmann::json& range		 = condition.at("range");
				if (range.is_number()) return brightness == range.get<float>();
				return (!range.contains("min") || brightness >= range["min"].get<float>()) && (!range.contains("max") || brightness <= range["max"].get<float>());
			}
			return false; // minecraft:structure: no structures are generated
		}
	} // namespace

	int selectToSpawn(Level& level, const std::string& name, const BlockPos& pos) {
		const Registry* reg = registry(level, name);
		if (!reg) return -1;
		// PriorityProvider.select: every selector of every variant, highest priority first (stable), keeping those whose
		// condition holds at the highest priority that holds
		struct Unpacked {
			int				   variant;
			int				   priority;
			const nlohmann::json* condition;
		};
		std::vector<Unpacked> entries;
		for (size_t v = 0; v < reg->size(); v++) {
			for (const Selector& s : (*reg)[v].selectors) entries.push_back({static_cast<int>(v), s.priority, &s.condition});
		}
		std::stable_sort(entries.begin(), entries.end(), [](const Unpacked& a, const Unpacked& b) { return a.priority > b.priority; });
		std::vector<int> selected;
		int				 best = std::numeric_limits<int>::min();
		for (const Unpacked& e : entries) {
			if (e.priority < best) continue;
			if (!test(level, *e.condition, pos)) continue;
			best = e.priority;
			selected.push_back(e.variant);
		}
		if (selected.empty()) return -1;
		// Util.getRandomSafe
		return selected[level.random().nextInt(static_cast<int>(selected.size()))];
	}

	int id(Level& level, const std::string& name, const std::string& variant) {
		const Registry* reg = registry(level, name);
		if (!reg) return -1;
		for (size_t v = 0; v < reg->size(); v++) {
			if ((*reg)[v].name == variant) return static_cast<int>(v);
		}
		return -1;
	}

	std::string name(Level& level, const std::string& name, int id) {
		const Registry* reg = registry(level, name);
		return reg && id >= 0 && id < static_cast<int>(reg->size()) ? (*reg)[id].name : "";
	}
} // namespace Variants
