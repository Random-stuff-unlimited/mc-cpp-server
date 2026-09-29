#include "world/item/ItemDamage.hpp"

#include "PacketIds.hpp"
#include "data/GameData.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/item/Components.hpp"
#include "world/item/Enchantments.hpp"

namespace {
	// EntityEvent: the item in this slot broke (its particles)
	int breakEvent(int slot) {
		if (slot == PlayerInventory::OFFHAND) return 48;
		if (slot >= 5 && slot <= 8) return 49 + (8 - slot) + 1; // Feet 52, legs 51, chest 50, head 49
		return 47;												// Main hand
	}

	int readVarInt(const std::vector<uint8_t>& data) {
		uint32_t result = 0;
		for (size_t i = 0, shift = 0; i < data.size() && shift < 35; i++, shift += 7) {
			result |= static_cast<uint32_t>(data[i] & 0x7F) << shift;
			if (!(data[i] & 0x80)) break;
		}
		return static_cast<int>(result);
	}
} // namespace

namespace ItemDamage {

	int maxDamage(const GameData& gameData, const ItemStack& stack) {
		if (stack.isEmpty()) return 0;
		if (Components::get(stack, gameData, "minecraft:unbreakable")) return 0;
		if (std::optional<std::vector<uint8_t>> value = Components::get(stack, gameData, "minecraft:max_damage")) return readVarInt(*value);
		const GameData::ItemProperties* props = gameData.getItemProperties(stack.item);
		return props ? props->maxDamage : 0;
	}

	int damage(const GameData& gameData, const ItemStack& stack) {
		std::optional<std::vector<uint8_t>> value = Components::get(stack, gameData, "minecraft:damage");
		return value ? readVarInt(*value) : 0;
	}

	void setDamage(const GameData& gameData, ItemStack& stack, int value) {
		Buffer encoded;
		encoded.writeVarInt(std::max(0, value));
		Components::set(stack, gameData, "minecraft:damage", value > 0 ? std::optional<std::vector<uint8_t>>(encoded.getData()) : std::nullopt);
	}

	void hurtAndBreak(Level& level, ItemStack& stack, int amount, Player* player, int slot) {
		const GameData& data = level.gameData();
		int				max	 = maxDamage(data, stack);
		if (max <= 0 || (player && player->isCreative())) return;
		// EnchantmentHelper.processDurabilityChange: unbreaking may spare it
		amount = Enchantments::processDurabilityChange(level, stack, amount);
		if (amount <= 0) return;
		int newDamage = damage(data, stack) + amount;
		setDamage(data, stack, newDamage);
		if (newDamage < max) return;
		// Broken: gone, with its sound and particles
		stack.shrink(1);
		if (stack.isEmpty()) stack = ItemStack{};
		if (player) {
			level.playSoundAt(nullptr, player->getX(), player->getY(), player->getZ(), "minecraft:entity.item.break", Level::SoundSource::Players, 0.8F,
							  0.8F + level.random().nextFloat() * 0.4F);
			Buffer event;
			event.writeInt(player->getPlayerID());
			event.writeByte(static_cast<int8_t>(breakEvent(slot)));
			level.server().getPlayerTracker().broadcast(player, PacketId::Play::Clientbound::ENTITY_EVENT, event, true);
		}
	}

} // namespace ItemDamage
