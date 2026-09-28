#include "world/item/Components.hpp"

#include "data/GameData.hpp"

#include <stdexcept>
#include <unordered_map>

namespace {
	// Bounds-checked reading of network bytes; throws on anything unexpected
	struct Reader {
		const std::vector<uint8_t>& data;
		size_t&						pos;

		void need(size_t count) const {
			if (pos + count > data.size()) throw std::runtime_error("component too short");
		}
		uint8_t u8() {
			need(1);
			return data[pos++];
		}
		void	skip(size_t count) {
			need(count);
			pos += count;
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
		void string() { skip(static_cast<size_t>(varint())); }
		bool boolean() { return u8() != 0; }

		// Network NBT: a tag type, then its payload (no root name). TAG_END alone is "no tag"
		void nbt() {
			uint8_t type = u8();
			if (type != 0) nbtPayload(type);
		}
		void nbtPayload(uint8_t type) {
			switch (type) {
			case 1: // Byte
				skip(1);
				break;
			case 2: // Short
				skip(2);
				break;
			case 3: // Int
			case 5: // Float
				skip(4);
				break;
			case 4: // Long
			case 6: // Double
				skip(8);
				break;
			case 7: // Byte array
				skip(static_cast<size_t>(int32()));
				break;
			case 8: // String
				skip(u16());
				break;
			case 9: { // List
				uint8_t element = u8();
				int32_t count	= int32();
				for (int32_t i = 0; i < count; i++) nbtPayload(element);
				break;
			}
			case 10: // Compound: named tags until TAG_END
				for (uint8_t child = u8(); child != 0; child = u8()) {
					skip(u16());
					nbtPayload(child);
				}
				break;
			case 11: // Int array
				skip(static_cast<size_t>(int32()) * 4);
				break;
			case 12: // Long array
				skip(static_cast<size_t>(int32()) * 8);
				break;
			default:
				throw std::runtime_error("bad NBT tag");
			}
		}
		uint16_t u16() {
			need(2);
			uint16_t value = static_cast<uint16_t>(data[pos] << 8 | data[pos + 1]);
			pos += 2;
			return value;
		}
		int32_t int32() {
			need(4);
			uint32_t value = static_cast<uint32_t>(data[pos]) << 24 | data[pos + 1] << 16 | data[pos + 2] << 8 | data[pos + 3];
			pos += 4;
			return static_cast<int32_t>(value);
		}
	};

	void writeVarInt(std::vector<uint8_t>& out, int32_t signedValue) {
		uint32_t value = static_cast<uint32_t>(signedValue);
		while (value >= 0x80) {
			out.push_back(static_cast<uint8_t>(value | 0x80));
			value >>= 7;
		}
		out.push_back(static_cast<uint8_t>(value));
	}

	void writeStack(std::vector<uint8_t>& out, const ItemStack& stack) {
		if (stack.isEmpty()) {
			writeVarInt(out, 0);
			return;
		}
		writeVarInt(out, stack.count);
		writeVarInt(out, stack.item);
		if (stack.components.empty()) {
			out.push_back(0);
			out.push_back(0);
		} else {
			out.insert(out.end(), stack.components.begin(), stack.components.end());
		}
	}

	void skipPatch(Reader& in, const GameData& gameData);

	// Reads a Slot, measuring its component patch
	ItemStack readStack(Reader& in, const GameData& gameData, bool optional) {
		int count = in.varint();
		if (count <= 0) {
			if (!optional) throw std::runtime_error("empty stack");
			return {};
		}
		ItemStack stack;
		stack.count	  = count;
		stack.item	  = in.varint();
		size_t start  = in.pos;
		skipPatch(in, gameData);
		stack.components.assign(in.data.begin() + static_cast<std::ptrdiff_t>(start), in.data.begin() + static_cast<std::ptrdiff_t>(in.pos));
		if (stack.components == std::vector<uint8_t>{0, 0}) stack.components.clear();
		return stack;
	}

	// Skips one component value of a known type; throws for the others
	void skipValue(Reader& in, const std::string& type, const GameData& gameData) {
		static const std::unordered_map<std::string, int> kinds = {
				// 0 varint, 1 nothing, 2 bool, 3 NBT, 4 int, 5 float, 6 string
				{"minecraft:max_stack_size", 0},	 {"minecraft:max_damage", 0},		   {"minecraft:damage", 0},
				{"minecraft:repair_cost", 0},		 {"minecraft:rarity", 0},			   {"minecraft:map_id", 0},
				{"minecraft:map_post_processing", 0}, {"minecraft:base_color", 0},		   {"minecraft:ominous_bottle_amplifier", 0},
				{"minecraft:unbreakable", 1},		 {"minecraft:creative_slot_lock", 1}, {"minecraft:glider", 1},
				{"minecraft:enchantment_glint_override", 2}, {"minecraft:custom_name", 3}, {"minecraft:item_name", 3},
				{"minecraft:bucket_entity_data", 3}, {"minecraft:dyed_color", 4},		   {"minecraft:map_color", 4},
				{"minecraft:potion_duration_scale", 5}, {"minecraft:item_model", 6},	   {"minecraft:tooltip_style", 6},
				{"minecraft:note_block_sound", 6},
		};
		auto kind = kinds.find(type);
		if (kind != kinds.end()) {
			switch (kind->second) {
			case 0:
				in.varint();
				return;
			case 1:
				return;
			case 2:
				in.skip(1);
				return;
			case 3:
				in.nbt();
				return;
			case 4:
			case 5:
				in.skip(4);
				return;
			default:
				in.string();
				return;
			}
		}
		if (type == "minecraft:lore") {
			for (int i = in.varint(); i > 0; i--) in.nbt();
		} else if (type == "minecraft:enchantments" || type == "minecraft:stored_enchantments") {
			for (int i = in.varint(); i > 0; i--) {
				in.varint(); // Enchantment
				in.varint(); // Level
			}
		} else if (type == "minecraft:tooltip_display") {
			in.skip(1);
			for (int i = in.varint(); i > 0; i--) in.varint();
		} else if (type == "minecraft:block_state") {
			for (int i = in.varint(); i > 0; i--) {
				in.string();
				in.string();
			}
		} else if (type == "minecraft:potion_contents") {
			if (in.boolean()) in.varint(); // Potion
			if (in.boolean()) in.skip(4);  // Custom color
			if (in.varint() != 0) throw std::runtime_error("custom effects");
			if (in.boolean()) in.string(); // Custom name
		} else if (type == "minecraft:suspicious_stew_effects") {
			for (int i = in.varint(); i > 0; i--) {
				in.varint(); // Effect
				in.varint(); // Duration
			}
		} else if (type == "minecraft:container") {
			for (int i = in.varint(); i > 0; i--) readStack(in, gameData, true);
		} else if (type == "minecraft:bundle_contents") {
			for (int i = in.varint(); i > 0; i--) readStack(in, gameData, false);
		} else {
			throw std::runtime_error("unknown component " + type);
		}
	}

	void skipPatch(Reader& in, const GameData& gameData) {
		int added	= in.varint();
		int removed = in.varint();
		for (int i = 0; i < added; i++) skipValue(in, gameData.getStaticName("minecraft:data_component_type", in.varint()), gameData);
		for (int i = 0; i < removed; i++) in.varint();
	}
} // namespace

// ----- ComponentPatch -----

std::optional<ComponentPatch> ComponentPatch::parse(const std::vector<uint8_t>& raw, const GameData& gameData) {
	ComponentPatch patch;
	if (raw.empty()) return patch;
	size_t pos = 0;
	Reader in{raw, pos};
	try {
		int added	= in.varint();
		int removed = in.varint();
		for (int i = 0; i < added; i++) {
			int	   type	 = in.varint();
			size_t start = pos;
			skipValue(in, gameData.getStaticName("minecraft:data_component_type", type), gameData);
			patch.added.emplace_back(type, std::vector<uint8_t>(raw.begin() + static_cast<std::ptrdiff_t>(start), raw.begin() + static_cast<std::ptrdiff_t>(pos)));
		}
		for (int i = 0; i < removed; i++) patch.removed.push_back(in.varint());
	} catch (const std::exception&) {
		return std::nullopt;
	}
	return patch;
}

std::vector<uint8_t> ComponentPatch::encode() const {
	std::vector<uint8_t> out;
	if (added.empty() && removed.empty()) return out;
	writeVarInt(out, static_cast<int32_t>(added.size()));
	writeVarInt(out, static_cast<int32_t>(removed.size()));
	for (const auto& [type, value] : added) {
		writeVarInt(out, type);
		out.insert(out.end(), value.begin(), value.end());
	}
	for (int type : removed) writeVarInt(out, type);
	return out;
}

const std::vector<uint8_t>* ComponentPatch::get(int type) const {
	for (const auto& [id, value] : added) {
		if (id == type) return &value;
	}
	return nullptr;
}

void ComponentPatch::set(int type, std::vector<uint8_t> value) {
	for (auto& [id, existing] : added) {
		if (id == type) {
			existing = std::move(value);
			return;
		}
	}
	added.emplace_back(type, std::move(value));
}

void ComponentPatch::erase(int type) {
	for (size_t i = 0; i < added.size(); i++) {
		if (added[i].first == type) {
			added.erase(added.begin() + static_cast<std::ptrdiff_t>(i));
			return;
		}
	}
}

// ----- Components -----

namespace Components {
	int typeId(const GameData& gameData, const std::string& name) { return gameData.getStaticId("minecraft:data_component_type", name); }

	std::vector<uint8_t> encodeContainer(const std::vector<ItemStack>& items) {
		size_t used = items.size();
		while (used > 0 && items[used - 1].isEmpty()) used--;
		std::vector<uint8_t> out;
		writeVarInt(out, static_cast<int32_t>(used));
		for (size_t i = 0; i < used; i++) writeStack(out, items[i]);
		return out;
	}

	std::optional<std::vector<ItemStack>> decodeContainer(const std::vector<uint8_t>& value, const GameData& gameData) {
		size_t pos = 0;
		Reader in{value, pos};
		try {
			std::vector<ItemStack> items;
			for (int i = in.varint(); i > 0; i--) items.push_back(readStack(in, gameData, true));
			return items;
		} catch (const std::exception&) {
			return std::nullopt;
		}
	}

	std::vector<uint8_t> encodePotion(int potion) {
		std::vector<uint8_t> out;
		out.push_back(1); // Potion present
		writeVarInt(out, potion);
		out.push_back(0); // No custom color
		writeVarInt(out, 0); // No custom effects
		out.push_back(0); // No custom name
		return out;
	}

	int decodePotion(const std::vector<uint8_t>& value) {
		size_t pos = 0;
		Reader in{value, pos};
		try {
			return in.boolean() ? in.varint() : -1;
		} catch (const std::exception&) {
			return -1;
		}
	}

	std::optional<std::vector<uint8_t>> get(const ItemStack& stack, const GameData& gameData, const std::string& name) {
		std::optional<ComponentPatch> patch = ComponentPatch::parse(stack.components, gameData);
		if (!patch) return std::nullopt;
		const std::vector<uint8_t>* value = patch->get(typeId(gameData, name));
		return value ? std::optional<std::vector<uint8_t>>(*value) : std::nullopt;
	}

	bool set(ItemStack& stack, const GameData& gameData, const std::string& name, std::optional<std::vector<uint8_t>> value) {
		std::optional<ComponentPatch> patch = ComponentPatch::parse(stack.components, gameData);
		if (!patch) return false;
		int type = typeId(gameData, name);
		if (value) {
			patch->set(type, std::move(*value));
		} else {
			patch->erase(type);
		}
		stack.components = patch->encode();
		return true;
	}

	std::optional<ItemStack> readStack(const std::vector<uint8_t>& data, size_t& pos, const GameData& gameData) {
		size_t start = pos;
		Reader in{data, pos};
		try {
			return ::readStack(in, gameData, true);
		} catch (const std::exception&) {
			pos = start;
			return std::nullopt;
		}
	}
} // namespace Components
