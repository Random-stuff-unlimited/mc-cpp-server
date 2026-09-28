#include "world/item/ItemNbt.hpp"

#include "data/GameData.hpp"
#include "lib/nbtParser.hpp"
#include "lib/nbtWriter.hpp"
#include "world/item/Components.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>

namespace {
	using nbt::Tag;
	using nbt::TagCompound;
	using nbt::TagList;
	using Compound = std::shared_ptr<TagCompound>;
	using List	   = std::shared_ptr<TagList>;

	// DyeColor and Rarity, by network id
	const std::array<const char*, 16> DYE_COLORS = {"white", "orange", "magenta", "light_blue", "yellow", "lime", "pink", "gray",
													"light_gray", "cyan", "purple", "blue", "brown", "green", "red", "black"};
	const std::array<const char*, 4>  RARITIES	 = {"common", "uncommon", "rare", "epic"};

	// Network bytes of one component value; throws past the end
	struct Reader {
		const std::vector<uint8_t>& data;
		size_t						pos = 0;

		void need(size_t count) const {
			if (pos + count > data.size()) throw std::runtime_error("component too short");
		}
		uint8_t u8() {
			need(1);
			return data[pos++];
		}
		int32_t varint() {
			uint32_t value = 0;
			for (int shift = 0; shift < 35; shift += 7) {
				uint8_t byte = u8();
				value |= static_cast<uint32_t>(byte & 0x7F) << shift;
				if (!(byte & 0x80)) return static_cast<int32_t>(value);
			}
			throw std::runtime_error("varint too long");
		}
		int32_t int32() {
			need(4);
			uint32_t value = static_cast<uint32_t>(data[pos]) << 24 | data[pos + 1] << 16 | data[pos + 2] << 8 | data[pos + 3];
			pos += 4;
			return static_cast<int32_t>(value);
		}
		std::string string() {
			size_t length = static_cast<size_t>(varint());
			need(length);
			std::string value(data.begin() + static_cast<std::ptrdiff_t>(pos), data.begin() + static_cast<std::ptrdiff_t>(pos + length));
			pos += length;
			return value;
		}
		Tag nbt() { return nbt::Parser().parseNetwork(data, pos); }
		bool done() const { return pos == data.size(); }
	};

	struct Writer {
		std::vector<uint8_t> out;
		void				 u8(uint8_t value) { out.push_back(value); }
		void				 varint(int32_t signedValue) {
			uint32_t value = static_cast<uint32_t>(signedValue);
			while (value >= 0x80) {
				out.push_back(static_cast<uint8_t>(value | 0x80));
				value >>= 7;
			}
			out.push_back(static_cast<uint8_t>(value));
		}
		void int32(int32_t value) {
			for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<uint8_t>(static_cast<uint32_t>(value) >> shift));
		}
		void string(const std::string& value) {
			varint(static_cast<int32_t>(value.size()));
			out.insert(out.end(), value.begin(), value.end());
		}
		void nbt(const Tag& tag) { nbt::Writer::writeNetwork(tag, out); }
	};

	// ----- NBT access, lenient like the codecs (any numeric type for a number) -----

	std::optional<int64_t> number(const Tag& tag) {
		return std::visit(
				[](const auto& value) -> std::optional<int64_t> {
					using T = std::decay_t<decltype(value)>;
					if constexpr (std::is_same_v<T, nbt::TagByte> || std::is_same_v<T, nbt::TagShort> || std::is_same_v<T, nbt::TagInt> ||
								  std::is_same_v<T, nbt::TagLong>) {
						return static_cast<int64_t>(value);
					} else if constexpr (std::is_same_v<T, nbt::TagFloat> || std::is_same_v<T, nbt::TagDouble>) {
						return static_cast<int64_t>(value);
					} else {
						return std::nullopt;
					}
				},
				tag.data);
	}
	int32_t intOf(const Tag& tag) {
		std::optional<int64_t> value = number(tag);
		if (!value) throw std::runtime_error("not a number");
		return static_cast<int32_t>(*value);
	}
	const std::string& stringOf(const Tag& tag) {
		if (const auto* value = std::get_if<nbt::TagString>(&tag.data)) return *value;
		throw std::runtime_error("not a string");
	}
	const TagCompound& compoundOf(const Tag& tag) {
		if (const auto* value = std::get_if<Compound>(&tag.data); value && *value) return **value;
		throw std::runtime_error("not a compound");
	}
	const TagList& listOf(const Tag& tag) {
		static const TagList empty;
		if (const auto* value = std::get_if<List>(&tag.data)) return *value ? **value : empty;
		throw std::runtime_error("not a list");
	}
	Tag compoundTag(TagCompound compound) { return Tag(std::make_shared<TagCompound>(std::move(compound))); }
	Tag listTag(TagList list) { return Tag(std::make_shared<TagList>(std::move(list))); }
	// Registries write ids with their namespace; a bare name is in minecraft's
	std::string withNamespace(const std::string& name) { return name.find(':') == std::string::npos ? "minecraft:" + name : name; }

	const GameData::Registry* syncedRegistry(const GameData& gameData, const std::string& name) {
		for (const GameData::Registry& registry : gameData.getSyncedRegistries()) {
			if (registry.name == name) return &registry;
		}
		return nullptr;
	}
	std::string staticName(const GameData& gameData, const std::string& registry, int id) {
		const std::string& name = gameData.getStaticName(registry, id);
		if (name.empty()) throw std::runtime_error("unknown " + registry + " id");
		return name;
	}
	int staticId(const GameData& gameData, const std::string& registry, const std::string& name) {
		int id = gameData.getStaticId(registry, withNamespace(name));
		if (id < 0) throw std::runtime_error("unknown " + registry + " " + name);
		return id;
	}
	template <size_t N> int indexOf(const std::array<const char*, N>& names, const std::string& name) {
		for (size_t i = 0; i < N; i++) {
			if (name == names[i]) return static_cast<int>(i);
		}
		throw std::runtime_error("unknown name " + name);
	}

	// ----- Components -----

	enum class Kind { VarInt, Unit, Bool, Nbt, Int, Float, Identifier, Dye, Rarity, Transient, Special };

	const std::map<std::string, Kind>& kinds() {
		static const std::map<std::string, Kind> KINDS = {
				{"minecraft:max_stack_size", Kind::VarInt},
				{"minecraft:max_damage", Kind::VarInt},
				{"minecraft:damage", Kind::VarInt},
				{"minecraft:repair_cost", Kind::VarInt},
				{"minecraft:map_id", Kind::VarInt},
				{"minecraft:ominous_bottle_amplifier", Kind::VarInt},
				{"minecraft:rarity", Kind::Rarity},
				{"minecraft:base_color", Kind::Dye},
				{"minecraft:unbreakable", Kind::Unit},
				{"minecraft:glider", Kind::Unit},
				{"minecraft:enchantment_glint_override", Kind::Bool},
				{"minecraft:custom_name", Kind::Nbt},
				{"minecraft:item_name", Kind::Nbt},
				{"minecraft:bucket_entity_data", Kind::Nbt},
				{"minecraft:dyed_color", Kind::Int},
				{"minecraft:map_color", Kind::Int},
				{"minecraft:potion_duration_scale", Kind::Float},
				{"minecraft:item_model", Kind::Identifier},
				{"minecraft:tooltip_style", Kind::Identifier},
				{"minecraft:note_block_sound", Kind::Identifier},
				// Network only (DataComponents.CREATIVE_SLOT_LOCK, MAP_POST_PROCESSING): vanilla doesn't save them
				{"minecraft:creative_slot_lock", Kind::Transient},
				{"minecraft:map_post_processing", Kind::Transient},
				{"minecraft:lore", Kind::Special},
				{"minecraft:enchantments", Kind::Special},
				{"minecraft:stored_enchantments", Kind::Special},
				{"minecraft:tooltip_display", Kind::Special},
				{"minecraft:block_state", Kind::Special},
				{"minecraft:potion_contents", Kind::Special},
				{"minecraft:suspicious_stew_effects", Kind::Special},
				{"minecraft:container", Kind::Special},
				{"minecraft:bundle_contents", Kind::Special},
		};
		return KINDS;
	}

	nbt::TagCompound saveStack(const ItemStack& stack, const GameData& gameData, bool& complete);
	ItemStack		 loadStack(const TagCompound& tag, const GameData& gameData, bool trustRaw);

	// One component value, network to NBT. Throws if it can't be converted
	Tag valueToNbt(const std::string& type, Kind kind, const std::vector<uint8_t>& value, const GameData& gameData, bool& complete) {
		Reader in{value};
		Tag	   result;
		switch (kind) {
		case Kind::VarInt:
			result = nbt::TagInt(in.varint());
			break;
		case Kind::Rarity:
			result = nbt::TagString(RARITIES.at(static_cast<size_t>(in.varint())));
			break;
		case Kind::Dye:
			result = nbt::TagString(DYE_COLORS.at(static_cast<size_t>(in.varint())));
			break;
		case Kind::Unit: // Unit.CODEC: {}
			result = compoundTag({});
			break;
		case Kind::Bool:
			result = nbt::TagByte(in.u8() != 0);
			break;
		case Kind::Nbt:
			result = in.nbt();
			break;
		case Kind::Int:
			result = nbt::TagInt(in.int32());
			break;
		case Kind::Float:
			result = std::bit_cast<float>(in.int32());
			break;
		case Kind::Identifier:
			result = nbt::TagString(in.string());
			break;
		case Kind::Transient:
			break;
		case Kind::Special:
			if (type == "minecraft:lore") { // List of text components
				TagList lines;
				for (int i = in.varint(); i > 0; i--) lines.push_back(in.nbt());
				result = listTag(std::move(lines));
			} else if (type == "minecraft:enchantments" || type == "minecraft:stored_enchantments") {
				// ItemEnchantments.CODEC: {"minecraft:sharpness": 5}
				const GameData::Registry* enchantments = syncedRegistry(gameData, "minecraft:enchantment");
				if (!enchantments) throw std::runtime_error("no enchantment registry");
				TagCompound levels;
				for (int i = in.varint(); i > 0; i--) {
					int id			  = in.varint();
					levels[enchantments->entries.at(static_cast<size_t>(id))] = nbt::TagInt(in.varint());
				}
				result = compoundTag(std::move(levels));
			} else if (type == "minecraft:tooltip_display") {
				TagCompound display;
				if (in.u8() != 0) display["hide_tooltip"] = nbt::TagByte(1);
				TagList hidden;
				for (int i = in.varint(); i > 0; i--) hidden.push_back(nbt::TagString(staticName(gameData, "minecraft:data_component_type", in.varint())));
				if (hidden.size() > 0) display["hidden_components"] = listTag(std::move(hidden));
				result = compoundTag(std::move(display));
			} else if (type == "minecraft:block_state") {
				TagCompound properties;
				for (int i = in.varint(); i > 0; i--) {
					std::string name  = in.string();
					properties[name] = nbt::TagString(in.string());
				}
				result = compoundTag(std::move(properties));
			} else if (type == "minecraft:potion_contents") {
				TagCompound contents;
				if (in.u8() != 0) contents["potion"] = nbt::TagString(staticName(gameData, "minecraft:potion", in.varint()));
				if (in.u8() != 0) contents["custom_color"] = nbt::TagInt(in.int32());
				if (in.varint() != 0) throw std::runtime_error("custom effects"); // MobEffectInstance isn't converted
				if (in.u8() != 0) contents["custom_name"] = nbt::TagString(in.string());
				result = compoundTag(std::move(contents));
			} else if (type == "minecraft:suspicious_stew_effects") {
				TagList effects;
				for (int i = in.varint(); i > 0; i--) {
					TagCompound effect;
					effect["id"] = nbt::TagString(staticName(gameData, "minecraft:mob_effect", in.varint()));
					int duration = in.varint();
					if (duration != 160) effect["duration"] = nbt::TagInt(duration); // Default of the codec
					effects.push_back(compoundTag(std::move(effect)));
				}
				result = listTag(std::move(effects));
			} else if (type == "minecraft:container" || type == "minecraft:bundle_contents") {
				std::optional<std::vector<ItemStack>> items = Components::decodeContainer(value, gameData);
				if (!items) throw std::runtime_error("unreadable items");
				TagList list;
				for (size_t slot = 0; slot < items->size(); slot++) {
					const ItemStack& stack = (*items)[slot];
					if (stack.isEmpty()) continue;
					TagCompound item = saveStack(stack, gameData, complete);
					if (type == "minecraft:bundle_contents") {
						list.push_back(compoundTag(std::move(item)));
					} else { // ItemContainerContents.Slot: {slot, item}
						TagCompound entry;
						entry["slot"] = nbt::TagInt(static_cast<int32_t>(slot));
						entry["item"] = compoundTag(std::move(item));
						list.push_back(compoundTag(std::move(entry)));
					}
				}
				result = listTag(std::move(list));
				return result;
			}
			break;
		}
		if (!in.done()) throw std::runtime_error("component longer than expected");
		return result;
	}

	// One component value, NBT to network. Throws if it can't be converted
	std::vector<uint8_t> valueFromNbt(const std::string& type, Kind kind, const Tag& tag, const GameData& gameData) {
		Writer out;
		switch (kind) {
		case Kind::VarInt:
			out.varint(intOf(tag));
			break;
		case Kind::Rarity:
			out.varint(indexOf(RARITIES, stringOf(tag)));
			break;
		case Kind::Dye:
			out.varint(indexOf(DYE_COLORS, stringOf(tag)));
			break;
		case Kind::Unit:
		case Kind::Transient:
			break;
		case Kind::Bool:
			out.u8(intOf(tag) != 0);
			break;
		case Kind::Nbt:
			out.nbt(tag);
			break;
		case Kind::Int:
			out.int32(intOf(tag));
			break;
		case Kind::Float: {
			float value;
			if (const auto* f = std::get_if<nbt::TagFloat>(&tag.data)) {
				value = *f;
			} else if (const auto* d = std::get_if<nbt::TagDouble>(&tag.data)) {
				value = static_cast<float>(*d);
			} else {
				value = static_cast<float>(intOf(tag));
			}
			out.int32(std::bit_cast<int32_t>(value));
			break;
		}
		case Kind::Identifier:
			out.string(withNamespace(stringOf(tag)));
			break;
		case Kind::Special:
			if (type == "minecraft:lore") {
				const TagList& lines = listOf(tag);
				out.varint(static_cast<int32_t>(lines.size()));
				for (const Tag& line : lines.data) out.nbt(nbt::unwrapListElement(line));
			} else if (type == "minecraft:enchantments" || type == "minecraft:stored_enchantments") {
				const TagCompound* levels = &compoundOf(tag);
				// Before 1.21.5: {levels: {...}, show_in_tooltip}
				if (levels->contains("levels")) levels = &compoundOf(levels->at("levels"));
				std::vector<std::pair<int, int>> entries;
				for (const auto& [name, level] : *levels) {
					int id = gameData.getSyncedId("minecraft:enchantment", withNamespace(name));
					if (id < 0) continue; // An enchantment this version doesn't have
					entries.emplace_back(id, intOf(level));
				}
				out.varint(static_cast<int32_t>(entries.size()));
				for (auto [id, level] : entries) {
					out.varint(id);
					out.varint(level);
				}
			} else if (type == "minecraft:tooltip_display") {
				const TagCompound& display = compoundOf(tag);
				out.u8(display.contains("hide_tooltip") && intOf(display.at("hide_tooltip")) != 0);
				std::vector<int> hidden;
				if (display.contains("hidden_components")) {
					for (const Tag& name : listOf(display.at("hidden_components")).data) {
						int id = gameData.getStaticId("minecraft:data_component_type", withNamespace(stringOf(name)));
						if (id >= 0) hidden.push_back(id);
					}
				}
				out.varint(static_cast<int32_t>(hidden.size()));
				for (int id : hidden) out.varint(id);
			} else if (type == "minecraft:block_state") {
				const TagCompound& properties = compoundOf(tag);
				out.varint(static_cast<int32_t>(properties.size()));
				for (const auto& [name, value] : properties) {
					out.string(name);
					out.string(stringOf(value));
				}
			} else if (type == "minecraft:potion_contents") {
				// PotionContents.CODEC also accepts the potion alone
				if (std::holds_alternative<nbt::TagString>(tag.data)) {
					TagCompound contents;
					contents["potion"] = tag;
					return valueFromNbt(type, kind, compoundTag(std::move(contents)), gameData);
				}
				const TagCompound& contents = compoundOf(tag);
				if (contents.contains("custom_effects") && listOf(contents.at("custom_effects")).size() > 0) {
					throw std::runtime_error("custom effects");
				}
				out.u8(contents.contains("potion"));
				if (contents.contains("potion")) out.varint(staticId(gameData, "minecraft:potion", stringOf(contents.at("potion"))));
				out.u8(contents.contains("custom_color"));
				if (contents.contains("custom_color")) out.int32(intOf(contents.at("custom_color")));
				out.varint(0);
				out.u8(contents.contains("custom_name"));
				if (contents.contains("custom_name")) out.string(stringOf(contents.at("custom_name")));
			} else if (type == "minecraft:suspicious_stew_effects") {
				const TagList& effects = listOf(tag);
				out.varint(static_cast<int32_t>(effects.size()));
				for (const Tag& element : effects.data) {
					const TagCompound& effect = compoundOf(element);
					out.varint(staticId(gameData, "minecraft:mob_effect", stringOf(effect.at("id"))));
					out.varint(effect.contains("duration") ? intOf(effect.at("duration")) : 160);
				}
			} else if (type == "minecraft:container") {
				std::vector<ItemStack> items;
				for (const Tag& element : listOf(tag).data) {
					const TagCompound& entry = compoundOf(element);
					int				   slot	 = intOf(entry.at("slot"));
					if (slot < 0 || slot >= 256) continue; // ItemContainerContents: 256 slots at most
					ItemStack stack = loadStack(compoundOf(entry.at("item")), gameData, false);
					if (stack.isEmpty()) continue;
					if (items.size() <= static_cast<size_t>(slot)) items.resize(static_cast<size_t>(slot) + 1);
					items[static_cast<size_t>(slot)] = std::move(stack);
				}
				return Components::encodeContainer(items);
			} else if (type == "minecraft:bundle_contents") {
				std::vector<ItemStack> items;
				for (const Tag& element : listOf(tag).data) {
					ItemStack stack = loadStack(compoundOf(element), gameData, false);
					if (!stack.isEmpty()) items.push_back(std::move(stack));
				}
				return Components::encodeContainer(items);
			}
			break;
		}
		return out.out;
	}

	nbt::TagCompound saveStack(const ItemStack& stack, const GameData& gameData, bool& complete) {
		TagCompound tag;
		tag["id"]	 = nbt::TagString(gameData.getStaticName("minecraft:item", stack.item));
		tag["count"] = nbt::TagInt(stack.count);
		if (!stack.components.empty()) {
			bool		stackComplete = true;
			TagCompound components	  = ItemNbt::componentsToNbt(stack.components, gameData, stackComplete);
			if (components.size() > 0) tag["components"] = compoundTag(std::move(components));
			if (!stackComplete) {
				tag[ItemNbt::RAW_COMPONENTS] = nbt::TagByteArray(stack.components.begin(), stack.components.end());
				complete					 = false;
			}
		}
		return tag;
	}

	ItemStack loadStack(const TagCompound& tag, const GameData& gameData, bool trustRaw) {
		if (!tag.contains("id")) return {};
		int item = gameData.getStaticId("minecraft:item", withNamespace(stringOf(tag.at("id"))));
		if (item <= 0) return {}; // Unknown in this version (or air)
		int count = tag.contains("count") ? intOf(tag.at("count")) : 1;
		if (count <= 0) return {};
		ItemStack stack(item, count);
		if (trustRaw && tag.contains(ItemNbt::RAW_COMPONENTS)) {
			if (const auto* raw = std::get_if<nbt::TagByteArray>(&tag.at(ItemNbt::RAW_COMPONENTS).data)) {
				stack.components.assign(raw->begin(), raw->end());
				return stack;
			}
		}
		if (tag.contains("components")) {
			if (const auto* components = std::get_if<Compound>(&tag.at("components").data); components && *components) {
				stack.components = ItemNbt::componentsFromNbt(**components, gameData);
			}
		}
		return stack;
	}
} // namespace

namespace ItemNbt {
	nbt::TagCompound save(const ItemStack& stack, const GameData& gameData) {
		bool complete = true;
		return saveStack(stack, gameData, complete);
	}

	ItemStack load(const nbt::TagCompound& tag, const GameData& gameData, bool trustRaw) {
		try {
			return loadStack(tag, gameData, trustRaw);
		} catch (const std::exception&) {
			return {}; // Not an item stack (wrong types)
		}
	}

	nbt::TagCompound componentsToNbt(const std::vector<uint8_t>& patch, const GameData& gameData, bool& complete) {
		TagCompound					  components;
		std::optional<ComponentPatch> parsed = ComponentPatch::parse(patch, gameData);
		if (!parsed) {
			complete = false;
			return components;
		}
		for (const auto& [type, value] : parsed->added) {
			const std::string& name = gameData.getStaticName("minecraft:data_component_type", type);
			auto			   kind = kinds().find(name);
			if (kind == kinds().end()) {
				complete = false;
				continue;
			}
			if (kind->second == Kind::Transient) continue;
			try {
				components[name] = valueToNbt(name, kind->second, value, gameData, complete);
			} catch (const std::exception&) {
				complete = false;
			}
		}
		// A removed default component: "!minecraft:food": {}
		for (int type : parsed->removed) {
			const std::string& name = gameData.getStaticName("minecraft:data_component_type", type);
			if (name.empty()) {
				complete = false;
				continue;
			}
			components["!" + name] = compoundTag({});
		}
		return components;
	}

	std::vector<uint8_t> componentsFromNbt(const nbt::TagCompound& components, const GameData& gameData) {
		ComponentPatch patch;
		for (const auto& [key, value] : components) {
			bool		removed = !key.empty() && key[0] == '!';
			std::string name	= withNamespace(removed ? key.substr(1) : key);
			int			type	= Components::typeId(gameData, name);
			if (type < 0) continue; // Removed from the game
			if (removed) {
				patch.removed.push_back(type);
				continue;
			}
			auto kind = kinds().find(name);
			if (kind == kinds().end() || kind->second == Kind::Transient) continue; // Not understood: dropped
			try {
				patch.set(type, valueFromNbt(name, kind->second, value, gameData));
			} catch (const std::exception&) {
				// Unreadable value: the item stays, without it
			}
		}
		return patch.encode();
	}
} // namespace ItemNbt
