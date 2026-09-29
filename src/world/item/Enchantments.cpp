#include "world/item/Enchantments.hpp"

#include "data/GameData.hpp"
#include "lib/JavaRandom.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/Survival.hpp"
#include "world/entity/LivingEntity.hpp"
#include "network/buffer.hpp"
#include "world/item/Components.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

using json = nlohmann::json;

namespace {
	// Definitions by registry id, built once per game data
	const std::vector<const json*>& definitions(const GameData& gameData) {
		static const GameData*			cached = nullptr;
		static std::vector<const json*> byId;
		if (cached != &gameData) {
			cached			  = &gameData;
			const json& data = gameData.getEnchantmentData();
			byId.clear();
			if (data.contains("enchantments")) {
				for (const auto& [name, definition] : data.at("enchantments").items()) {
					int id = gameData.getSyncedId("minecraft:enchantment", name);
					if (id < 0) continue;
					if (static_cast<size_t>(id) >= byId.size()) byId.resize(id + 1, nullptr);
					byId[id] = &definition;
				}
			}
		}
		return byId;
	}

	bool readVarInt(const std::vector<uint8_t>& data, size_t& pos, int& out) {
		uint32_t result = 0;
		for (int shift = 0; shift < 35; shift += 7) {
			if (pos >= data.size()) return false;
			uint8_t byte = data[pos++];
			result |= static_cast<uint32_t>(byte & 0x7F) << shift;
			if (!(byte & 0x80)) {
				out = static_cast<int>(result);
				return true;
			}
		}
		return false;
	}

	// An item predicate's "items": an item, a list, or a #tag
	bool itemsMatch(const GameData& data, const json& items, int item) {
		if (items.is_array()) {
			for (const json& entry : items) {
				if (itemsMatch(data, entry, item)) return true;
			}
			return false;
		}
		std::string name = items.get<std::string>();
		if (!name.empty() && name[0] == '#') return data.isInTag("minecraft:item", name.substr(1), item);
		return data.getStaticId("minecraft:item", name) == item;
	}

	bool entityMatches(const GameData& data, const json& predicate, Actor* actor) {
		if (!actor) return false;
		for (const auto& [key, value] : predicate.items()) {
			if (key == "type") {
				std::string type = value.get<std::string>();
				bool		ok	 = type[0] == '#' ? data.isInTag("minecraft:entity_type", type.substr(1), actor->typeId())
												  : data.getStaticId("minecraft:entity_type", type) == actor->typeId();
				if (!ok) return false;
			} else if (key == "flags") {
				for (const auto& [flag, expected] : value.items()) {
					bool actual = false;
					if (flag == "is_on_fire") {
						if (LivingEntity* living = actor->asLiving()) actual = living->isOnFire();
						if (Player* player = actor->asPlayer()) actual = player->survival().remainingFireTicks > 0;
					} else if (flag == "is_baby") {
						actual = actor->asLiving() && actor->asLiving()->isBaby();
					} else {
						return false;
					}
					if (actual != expected.get<bool>()) return false;
				}
			} else {
				return false; // Not ported: such effects don't apply
			}
		}
		return true;
	}

	bool damageMatches(const GameData& data, const json& predicate, const Combat::DamageSource* source) {
		if (!source) return false;
		int type = data.getSyncedId("minecraft:damage_type", source->type);
		for (const auto& [key, value] : predicate.items()) {
			if (key == "tags") {
				for (const json& tag : value) {
					bool in = data.isInTag("minecraft:damage_type", tag.at("id").get<std::string>(), type);
					if (in != tag.value("expected", true)) return false;
				}
			} else if (key == "is_direct") {
				if (source->isDirect() != value.get<bool>()) return false;
			} else if (key == "direct_entity") {
				if (!entityMatches(data, value, source->directEntity())) return false;
			} else if (key == "source_entity") {
				if (!entityMatches(data, value, source->causing)) return false;
			} else {
				return false;
			}
		}
		return true;
	}

	using namespace Enchantments;

	// EnchantmentEntityEffect.apply for the effects of post_attack and co: what is ported
	void applyEntityEffect(const json& effect, int level, Actor& target, Context& context) {
		std::string type = effect.value("type", "");
		if (type == "minecraft:all_of") {
			for (const json& inner : effect.at("effects")) applyEntityEffect(inner, level, target, context);
		} else if (type == "minecraft:ignite") {
			int ticks = static_cast<int>(levelValue(effect.at("duration"), level) * 20.0F); // igniteForSeconds
			target.igniteForTicks(ticks);
		} else if (type == "minecraft:damage_entity") {
			JavaRandom& random = context.level.random();
			float		min = levelValue(effect.at("min_damage"), level), max = levelValue(effect.at("max_damage"), level);
			float		amount = min + random.nextFloat() * (max - min); // Mth.randomBetween
			target.hurtServer({effect.at("damage_type").get<std::string>(), context.attacker, nullptr, std::nullopt}, amount);
		} else if (type == "minecraft:play_sound") {
			const Vec3& at = target.position();
			std::string sound = effect.at("sound").is_string() ? effect.at("sound").get<std::string>() : effect.at("sound").value("sound_id", "");
			context.level.playSoundAt(nullptr, at.x, at.y, at.z, sound, Level::SoundSource::Neutral, 1.0F, 1.0F);
		}
		// Other entity effects (apply_mob_effect, summon_entity, explode...) come with their features
	}

	// Runs an enchantment's effects of a component
	template <typename F> void forEachEffect(const GameData& data, const ItemStack& stack, const std::string& component, F f) {
		for (auto [id, level] : Enchantments::of(data, stack)) {
			const json* definition = Enchantments::definition(data, id);
			if (!definition || !definition->contains("effects")) continue;
			const json& effects = definition->at("effects");
			auto		it		= effects.find(component);
			if (it == effects.end()) continue;
			for (const json& entry : *it) f(entry, level);
		}
	}
// EnchantmentHelper.getEnchantmentsComponentType: an enchanted book stores them in minecraft:stored_enchantments
	const char* componentOf(const GameData& gameData, const ItemStack& stack) {
		return gameData.getStaticName("minecraft:item", stack.item) == "minecraft:enchanted_book" ? "minecraft:stored_enchantments" : "minecraft:enchantments";
	}
} // namespace

namespace Enchantments {

	std::vector<std::pair<int, int>> of(const GameData& gameData, const ItemStack& stack) {
		std::vector<std::pair<int, int>> result;
		if (stack.isEmpty() || stack.components.empty()) return result;
		std::optional<std::vector<uint8_t>> value = Components::get(stack, gameData, componentOf(gameData, stack));
		if (!value) return result;
		size_t pos	 = 0;
		int	   count = 0;
		if (!readVarInt(*value, pos, count)) return result;
		for (int i = 0; i < count; i++) {
			int id, level;
			if (!readVarInt(*value, pos, id) || !readVarInt(*value, pos, level)) break;
			result.emplace_back(id, level);
		}
		return result;
	}

	int level(const GameData& gameData, const ItemStack& stack, const std::string& enchantment) {
		int wanted = gameData.getSyncedId("minecraft:enchantment", enchantment);
		for (auto [id, level] : of(gameData, stack)) {
			if (id == wanted) return level;
		}
		return 0;
	}

	const json* definition(const GameData& gameData, int enchantment) {
		const auto& byId = definitions(gameData);
		return enchantment >= 0 && static_cast<size_t>(enchantment) < byId.size() ? byId[enchantment] : nullptr;
	}

	float levelValue(const json& value, int level) {
		if (value.is_number()) return value.get<float>();
		std::string type = value.value("type", "");
		if (type == "minecraft:linear") return value.value("base", 0.0F) + value.value("per_level_above_first", 0.0F) * (level - 1);
		if (type == "minecraft:clamped") {
			return std::clamp(levelValue(value.at("value"), level), value.value("min", -1.0E30F), value.value("max", 1.0E30F));
		}
		if (type == "minecraft:fraction") return levelValue(value.at("numerator"), level) / levelValue(value.at("denominator"), level);
		if (type == "minecraft:levels_squared") return static_cast<float>(level * level) + value.value("added", 0.0F);
		if (type == "minecraft:lookup") {
			const json& values = value.at("values");
			if (level - 1 >= 0 && static_cast<size_t>(level - 1) < values.size()) return values[level - 1].get<float>();
			return levelValue(value.at("fallback"), level);
		}
		return 0.0F;
	}

	float applyValueEffect(const json& effect, float value, int level, JavaRandom& random) {
		std::string type = effect.value("type", "");
		if (type == "minecraft:add") return value + levelValue(effect.at("value"), level);
		if (type == "minecraft:multiply") return value * levelValue(effect.at("factor"), level);
		if (type == "minecraft:set") return levelValue(effect.at("value"), level);
		if (type == "minecraft:remove_binomial") {
			// Each point has `chance` to be removed
			float chance = levelValue(effect.at("chance"), level);
			int	  removed = 0;
			for (int i = 0; i < static_cast<int>(value); i++) {
				if (random.nextFloat() < chance) removed++;
			}
			return value - removed;
		}
		if (type == "minecraft:all_of") {
			for (const json& inner : effect.at("effects")) value = applyValueEffect(inner, value, level, random);
			return value;
		}
		return value;
	}

	bool conditionsPass(const json* requirements, Context& context) {
		if (!requirements || requirements->is_null()) return true;
		const json& condition = *requirements;
		if (condition.is_array()) {
			for (const json& inner : condition) {
				if (!conditionsPass(&inner, context)) return false;
			}
			return true;
		}
		const GameData& data = context.level.gameData();
		std::string		type = condition.value("condition", "");
		if (type == "minecraft:all_of") return conditionsPass(&condition.at("terms"), context);
		if (type == "minecraft:any_of") {
			for (const json& inner : condition.at("terms")) {
				if (conditionsPass(&inner, context)) return true;
			}
			return false;
		}
		if (type == "minecraft:inverted") return !conditionsPass(&condition.at("term"), context);
		if (type == "minecraft:random_chance") return context.level.random().nextFloat() < levelValue(condition.at("chance"), context.enchantmentLevel);
		if (type == "minecraft:match_tool") {
			if (!context.tool || context.tool->isEmpty()) return false;
			const json& predicate = condition.value("predicate", json::object());
			for (const auto& [key, value] : predicate.items()) {
				if (key == "items") {
					if (!itemsMatch(data, value, context.tool->item)) return false;
				} else {
					return false;
				}
			}
			return true;
		}
		if (type == "minecraft:damage_source_properties") return damageMatches(data, condition.value("predicate", json::object()), context.damage);
		if (type == "minecraft:entity_properties") {
			std::string which = condition.value("entity", "this");
			Actor*		actor = which == "this" ? context.entity : which == "attacker" ? context.attacker : context.directAttacker;
			return entityMatches(data, condition.value("predicate", json::object()), actor);
		}
		return false; // Conditions not ported: the effect doesn't apply
	}

	float modifyItem(const ItemStack& stack, const std::string& component, float value, Context& context) {
		forEachEffect(context.level.gameData(), stack, component, [&](const json& entry, int level) {
			context.enchantmentLevel = level;
			if (!conditionsPass(entry.contains("requirements") ? &entry.at("requirements") : nullptr, context)) return;
			value = applyValueEffect(entry.at("effect"), value, level, context.level.random());
		});
		return value;
	}

	int processDurabilityChange(Level& level, const ItemStack& stack, int amount) {
		Context context{level};
		context.tool = &stack;
		return static_cast<int>(modifyItem(stack, "minecraft:item_damage", static_cast<float>(amount), context));
	}

	float modifyDamage(Level& level, const ItemStack& weapon, Actor& target, const Combat::DamageSource& source, float damage) {
		Context context{level};
		context.tool		   = &weapon;
		context.entity		   = &target;
		context.damage		   = &source;
		context.attacker	   = source.causing;
		context.directAttacker = source.directEntity();
		return modifyItem(weapon, "minecraft:damage", damage, context);
	}

	float modifyKnockback(Level& level, const ItemStack& weapon, Actor& target, const Combat::DamageSource& source, float knockback) {
		Context context{level};
		context.tool		   = &weapon;
		context.entity		   = &target;
		context.damage		   = &source;
		context.attacker	   = source.causing;
		context.directAttacker = source.directEntity();
		return modifyItem(weapon, "minecraft:knockback", knockback, context);
	}

	float damageProtection(Level& level, const std::vector<ItemStack>& armor, Actor& wearer, const Combat::DamageSource& source) {
		float protection = 0.0F;
		for (const ItemStack& piece : armor) {
			Context context{level};
			context.tool		   = &piece;
			context.entity		   = &wearer;
			context.damage		   = &source;
			context.attacker	   = source.causing;
			context.directAttacker = source.directEntity();
			protection			   = modifyItem(piece, "minecraft:damage_protection", protection, context);
		}
		return protection;
	}

	void doPostAttackEffects(Level& level, const ItemStack& weapon, Actor& attacker, Actor& victim, const Combat::DamageSource& source) {
		forEachEffect(level.gameData(), weapon, "minecraft:post_attack", [&](const json& entry, int enchantmentLevel) {
			// TargetedConditionalEffect: the attacker's weapon affects the victim (or the attacker)
			if (entry.value("enchanted", "attacker") != "attacker") return;
			Context context{level};
			context.tool			 = &weapon;
			context.entity			 = &victim;
			context.damage			 = &source;
			context.attacker		 = source.causing;
			context.directAttacker	 = source.directEntity();
			context.enchantmentLevel = enchantmentLevel;
			if (!conditionsPass(entry.contains("requirements") ? &entry.at("requirements") : nullptr, context)) return;
			Actor& affected = entry.value("affected", "victim") == "victim" ? victim : attacker;
			applyEntityEffect(entry.at("effect"), enchantmentLevel, affected, context);
		});
	}

	float processEquipmentDropChance(Level& level, const ItemStack& weapon, Actor& victim, const Combat::DamageSource& source, float chance) {
		Context context{level};
		context.tool		   = &weapon;
		context.entity		   = &victim;
		context.damage		   = &source;
		context.attacker	   = source.causing;
		context.directAttacker = source.directEntity();
		return modifyItem(weapon, "minecraft:equipment_drops", chance, context);
	}

} // namespace Enchantments

namespace Enchantments {

	void set(const GameData& gameData, ItemStack& stack, const std::vector<std::pair<int, int>>& enchantments) {
		const char* component = componentOf(gameData, stack);
		if (enchantments.empty()) {
			Components::set(stack, gameData, component, std::nullopt);
			return;
		}
		Buffer encoded;
		encoded.writeVarInt(static_cast<int32_t>(enchantments.size()));
		for (auto [id, level] : enchantments) {
			encoded.writeVarInt(id);
			encoded.writeVarInt(level);
		}
		Components::set(stack, gameData, component, encoded.getData());
	}

	namespace {
		// A HolderSet of the enchantment registry: an id, a list or a #tag
		bool holderSetContains(const GameData& data, const json& set, int enchantment) {
			if (set.is_array()) {
				for (const json& entry : set) {
					if (holderSetContains(data, entry, enchantment)) return true;
				}
				return false;
			}
			std::string name = set.get<std::string>();
			if (!name.empty() && name[0] == '#') return data.isInTag("minecraft:enchantment", name.substr(1), enchantment);
			return data.getSyncedId("minecraft:enchantment", name) == enchantment;
		}
		// Enchantment.Cost.calculate
		int cost(const json& definition, const char* which, int level) {
			const json& c = definition.at(which);
			return c.value("base", 0) + c.value("per_level_above_first", 0) * (level - 1);
		}
		bool isPrimaryItem(const GameData& data, const json& definition, int item) {
			const json& items = definition.contains("primary_items") ? definition.at("primary_items") : definition.at("supported_items");
			return itemsMatch(data, items, item);
		}
	} // namespace

	bool areCompatible(const GameData& gameData, int a, int b) {
		if (a == b) return false;
		const json* da = definition(gameData, a);
		const json* db = definition(gameData, b);
		if (da && da->contains("exclusive_set") && holderSetContains(gameData, da->at("exclusive_set"), b)) return false;
		if (db && db->contains("exclusive_set") && holderSetContains(gameData, db->at("exclusive_set"), a)) return false;
		return true;
	}

	std::vector<std::pair<int, int>> selectEnchantment(const GameData& gameData, JavaRandom& random, const ItemStack& stack, int cost,
													   const std::vector<int>& candidates) {
		std::vector<std::pair<int, int>> result;
		const GameData::ItemProperties*	 props = gameData.getItemProperties(stack.item);
		if (!props || props->enchantability <= 0) return result;
		int value = props->enchantability;
		cost += 1 + random.nextInt(value / 4 + 1) + random.nextInt(value / 4 + 1);
		float spread = (random.nextFloat() + random.nextFloat() - 1.0F) * 0.15F;
		cost		 = std::max(1, static_cast<int>(std::lround(cost + cost * spread)));
		// getAvailableEnchantmentResults: the highest level each enchantment reaches at this cost
		bool book = gameData.getStaticName("minecraft:item", stack.item) == "minecraft:book";
		struct Instance {
			int id, level, weight;
		};
		std::vector<Instance> available;
		for (int id : candidates) {
			const json* d = definition(gameData, id);
			if (!d || (!book && !isPrimaryItem(gameData, *d, stack.item))) continue;
			for (int level = d->value("max_level", 1); level >= 1; level--) {
				if (cost >= Enchantments::cost_min(*d, level) && cost <= Enchantments::cost_max(*d, level)) {
					available.push_back({id, level, d->value("weight", 1)});
					break;
				}
			}
		}
		auto pick = [&]() -> std::optional<Instance> {
			int total = 0;
			for (const Instance& i : available) total += i.weight;
			if (total <= 0) return std::nullopt;
			int r = random.nextInt(total);
			for (const Instance& i : available) {
				r -= i.weight;
				if (r < 0) return i;
			}
			return std::nullopt;
		};
		if (available.empty()) return result;
		if (auto first = pick()) result.emplace_back(first->id, first->level);
		while (random.nextInt(50) <= cost) {
			if (!result.empty()) {
				int last = result.back().first;
				available.erase(std::remove_if(available.begin(), available.end(), [&](const Instance& i) { return !areCompatible(gameData, last, i.id); }),
								available.end());
			}
			if (available.empty()) break;
			if (auto next = pick()) result.emplace_back(next->id, next->level);
			cost /= 2;
		}
		return result;
	}

	std::vector<int> tagEntries(const GameData& gameData, const std::string& tag) {
		for (const GameData::RegistryTags& registry : gameData.getTags()) {
			if (registry.registry != "minecraft:enchantment") continue;
			for (const auto& [name, ids] : registry.tags) {
				if (name == tag) return ids;
			}
		}
		return {};
	}

	void enchantItemFromProvider(const GameData& gameData, ItemStack& stack, const std::string& provider, float specialMultiplier, JavaRandom& random) {
		const json& data = gameData.getEnchantmentData();
		if (!data.contains("providers") || !data.at("providers").contains(provider)) return;
		const json& p = data.at("providers").at(provider);
		std::vector<std::pair<int, int>> current = of(gameData, stack);
		auto upgrade = [&](int id, int level) {
			// ItemEnchantments.Mutable.upgrade: the higher level stays
			auto it = std::find_if(current.begin(), current.end(), [&](const auto& e) { return e.first == id; });
			if (it == current.end()) {
				current.emplace_back(id, level);
			} else {
				it->second = std::max(it->second, level);
			}
		};
		if (p.value("type", "") == "minecraft:single") {
			int id = gameData.getSyncedId("minecraft:enchantment", p.at("enchantment").get<std::string>());
			if (id >= 0) upgrade(id, p.value("level", 1));
			Enchantments::set(gameData, stack, current);
			return;
		}
		if (p.value("type", "") != "minecraft:by_cost_with_difficulty") return;
		std::string set	  = p.at("enchantments").get<std::string>();
		std::vector<int> candidates = set[0] == '#' ? tagEntries(gameData, set.substr(1)) : std::vector<int>{gameData.getSyncedId("minecraft:enchantment", set)};
		int minCost = p.value("min_cost", 0), span = p.value("max_cost_span", 0);
		int cost	= minCost + random.nextInt(static_cast<int>(specialMultiplier * span) + 1); // Mth.randomBetweenInclusive
		for (auto [id, level] : selectEnchantment(gameData, random, stack, cost, candidates)) upgrade(id, level);
		Enchantments::set(gameData, stack, current);
	}

	int cost_min(const json& definition, int level) { return cost(definition, "min_cost", level); }
	int cost_max(const json& definition, int level) { return cost(definition, "max_cost", level); }

} // namespace Enchantments
