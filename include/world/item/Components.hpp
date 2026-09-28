#ifndef COMPONENTS_HPP
#define COMPONENTS_HPP

#include "world/item/ItemStack.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class GameData;

// An item's component patch (DataComponentPatch as on the network) split into its components. Values stay encoded;
// only the components whose network format is known can be told apart: a patch with another one can't be split
// (and stays opaque in its ItemStack)
struct ComponentPatch {
	std::vector<std::pair<int, std::vector<uint8_t>>> added; // minecraft:data_component_type id, encoded value
	std::vector<int>								  removed;

	// nullopt if a component isn't known
	static std::optional<ComponentPatch> parse(const std::vector<uint8_t>& raw, const GameData& gameData);
	// The raw patch (empty when there is no change)
	std::vector<uint8_t> encode() const;

	const std::vector<uint8_t>* get(int type) const;
	void						set(int type, std::vector<uint8_t> value);
	void						erase(int type);
};

// Readers and writers of the components the server uses
namespace Components {
	// Id of a component type ("minecraft:container"), -1 if unknown
	int typeId(const GameData& gameData, const std::string& name);

	// minecraft:container (ItemContainerContents): the slots up to the last non-empty one
	std::vector<uint8_t>				  encodeContainer(const std::vector<ItemStack>& items);
	std::optional<std::vector<ItemStack>> decodeContainer(const std::vector<uint8_t>& value, const GameData& gameData);
	// minecraft:potion_contents with only a potion (minecraft:potion id), and the potion of one (-1 if none)
	std::vector<uint8_t> encodePotion(int potion);
	int					 decodePotion(const std::vector<uint8_t>& value);

	// The component of a stack, if its patch can be read
	std::optional<std::vector<uint8_t>> get(const ItemStack& stack, const GameData& gameData, const std::string& name);
	// Sets or removes one component of a stack; false if its patch can't be read
	bool set(ItemStack& stack, const GameData& gameData, const std::string& name, std::optional<std::vector<uint8_t>> value);

	// Reads one Slot (ItemStack.OPTIONAL_STREAM_CODEC) from data at pos; nullopt if its components can't be read
	std::optional<ItemStack> readStack(const std::vector<uint8_t>& data, size_t& pos, const GameData& gameData);
} // namespace Components

#endif
