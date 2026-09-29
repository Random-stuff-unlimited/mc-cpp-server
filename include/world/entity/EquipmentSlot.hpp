#ifndef EQUIPMENT_SLOT_HPP
#define EQUIPMENT_SLOT_HPP

#include "data/GameData.hpp"

#include <cstdint>
#include <string>

// Vanilla's EquipmentSlot, with its ids (EquipmentSlot.getId: the network and save order)
enum class EquipmentSlot : uint8_t { MainHand = 0, OffHand = 1, Feet = 2, Legs = 3, Chest = 4, Head = 5, Body = 6, Saddle = 7 };
constexpr int EQUIPMENT_SLOT_COUNT = 8;

namespace EquipmentSlots {
	inline EquipmentSlot fromGameData(GameData::EquipmentSlot slot) {
		switch (slot) {
		case GameData::EquipmentSlot::MainHand: return EquipmentSlot::MainHand;
		case GameData::EquipmentSlot::OffHand: return EquipmentSlot::OffHand;
		case GameData::EquipmentSlot::Head: return EquipmentSlot::Head;
		case GameData::EquipmentSlot::Chest: return EquipmentSlot::Chest;
		case GameData::EquipmentSlot::Legs: return EquipmentSlot::Legs;
		case GameData::EquipmentSlot::Feet: return EquipmentSlot::Feet;
		case GameData::EquipmentSlot::Body: return EquipmentSlot::Body;
		case GameData::EquipmentSlot::Saddle: return EquipmentSlot::Saddle;
		}
		return EquipmentSlot::MainHand;
	}
	inline bool isArmor(EquipmentSlot slot) { return slot >= EquipmentSlot::Feet && slot <= EquipmentSlot::Head; }
	// EquipmentSlotGroup.test: whether a modifier of this slot group ("any", "hand", "armor", "chest"...) applies in slot
	inline bool groupContains(const std::string& group, EquipmentSlot slot) {
		if (group == "any") return true;
		if (group == "hand") return slot == EquipmentSlot::MainHand || slot == EquipmentSlot::OffHand;
		if (group == "armor") return isArmor(slot);
		static const char* NAMES[EQUIPMENT_SLOT_COUNT] = {"mainhand", "offhand", "feet", "legs", "chest", "head", "body", "saddle"};
		return group == NAMES[static_cast<int>(slot)];
	}
} // namespace EquipmentSlots

#endif
