#ifndef ITEM_NBT_HPP
#define ITEM_NBT_HPP

#include "lib/nbt.hpp"
#include "world/item/ItemStack.hpp"

#include <cstdint>
#include <optional>
#include <vector>

class GameData;

// An ItemStack as vanilla saves it (ItemStack.CODEC): {id: "minecraft:stone", count: 3, components: {...}}.
//
// Our stacks keep their components as the network patch. The components whose network format is known are converted
// to and from their vanilla NBT form (DataComponentPatch.CODEC: "minecraft:damage": 5, "!minecraft:food": {} for a
// removed one). When the patch can't be converted entirely (a component the server doesn't understand), the raw
// patch is also kept under RAW_COMPONENTS, so nothing is lost on this server; vanilla ignores that key and loads the
// item with the components that could be converted.
namespace ItemNbt {
	inline constexpr const char* RAW_COMPONENTS = "mc-cpp-server:network_components";

	// The stack must not be empty
	nbt::TagCompound save(const ItemStack& stack, const GameData& gameData);
	// Air if the item doesn't exist in this version. Components that can't be read are dropped (the item stays).
	// trustRaw: the data was written by this server with this version's ids (same DataVersion), so RAW_COMPONENTS
	// can be used as is
	ItemStack load(const nbt::TagCompound& tag, const GameData& gameData, bool trustRaw);

	// Network component patch to the vanilla "components" compound. complete is false if some component couldn't
	// be converted (left out)
	nbt::TagCompound componentsToNbt(const std::vector<uint8_t>& patch, const GameData& gameData, bool& complete);
	// The vanilla "components" compound to a network patch; unknown or unreadable components are left out
	std::vector<uint8_t> componentsFromNbt(const nbt::TagCompound& components, const GameData& gameData);
} // namespace ItemNbt

#endif
