#include "Test.hpp"
#include "data/GameData.hpp"

#include <map>

namespace {
	const GameData& gameData() {
		static GameData data;
		static bool		loaded = false;
		if (!loaded) {
			data.load("resources/gamedata");
			loaded = true;
		}
		return data;
	}

	// "minecraft:oak_stairs[facing=north,half=top]" -> {facing: north, half: top}
	std::map<std::string, std::string> propertiesOf(const std::string& name) {
		std::map<std::string, std::string> properties;
		size_t							   open = name.find('[');
		if (open == std::string::npos) return properties;
		std::string list = name.substr(open + 1, name.size() - open - 2);
		size_t		start = 0;
		while (start < list.size()) {
			size_t end	 = list.find(',', start);
			if (end == std::string::npos) end = list.size();
			std::string pair = list.substr(start, end - start);
			size_t		equals = pair.find('=');
			properties[pair.substr(0, equals)] = pair.substr(equals + 1);
			start = end + 1;
		}
		return properties;
	}
} // namespace

// Every property of every state, read and changed to every value, against the string-based GameData
TEST(block_registry_matches_game_data) {
	const GameData&		 data	= gameData();
	const BlockRegistry& blocks = data.getBlocks();
	for (int state = 0; state < data.getBlockStateCount(); state++) {
		CHECK_EQ(blocks.blockOf(state), data.getBlockOfState(state));
		for (const auto& [name, value] : propertiesOf(data.getBlockStateName(state))) {
			int property = blocks.property(name);
			CHECK(property >= 0);
			CHECK_EQ(blocks.get(state, property), blocks.value(value));
		}
		// with() for every value any block uses for these properties
		for (const auto& [name, value] : propertiesOf(data.getBlockStateName(state))) {
			int property = blocks.property(name);
			for (const auto& [otherName, otherValue] : propertiesOf(data.getBlockStateName(blocks.cycle(state, property)))) {
				if (otherName != name) continue;
				CHECK_EQ(blocks.with(state, property, blocks.value(otherValue)), data.withProperty(state, name, otherValue));
			}
		}
	}
}

TEST(block_registry_typed_properties) {
	const GameData&		 data	= gameData();
	const BlockRegistry& blocks = data.getBlocks();
	int wire  = data.getDefaultBlockState("minecraft:redstone_wire");
	int power = blocks.property("power");
	CHECK_EQ(blocks.getInt(wire, power), 0);
	int powered15 = blocks.withInt(wire, power, 15);
	CHECK_EQ(blocks.getInt(powered15, power), 15);
	CHECK_EQ(powered15, data.withProperty(wire, "power", "15"));
	CHECK_EQ(blocks.withInt(wire, power, 16), -1);

	int lever	= data.getDefaultBlockState("minecraft:lever");
	int poweredP = blocks.property("powered");
	CHECK(!blocks.getBool(lever, poweredP));
	CHECK(blocks.getBool(blocks.withBool(lever, poweredP, true), poweredP));
	CHECK_EQ(blocks.with(data.getDefaultBlockState("minecraft:stone"), poweredP, blocks.value("true")), -1);

	CHECK(blocks.isAir(data.getDefaultBlockState("minecraft:air")));
	CHECK(blocks.isAir(data.getDefaultBlockState("minecraft:cave_air")));
	CHECK(!blocks.isAir(data.getDefaultBlockState("minecraft:stone")));
}
