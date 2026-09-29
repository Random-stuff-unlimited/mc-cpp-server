#include "world/Xp.hpp"

#include "data/GameData.hpp"
#include "network/PacketIds.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Level.hpp"

#include <algorithm>

namespace {
	// ServerPlayer.addExperienceLevels: the level-up sound when a multiple of 5 is reached, its pitch by level
	void levelUpSound(Server& server, Player& player, int newLevel) {
		if (newLevel <= 0 || newLevel % 5 != 0) return;
		Level& level = player.level() ? *player.level() : server.getLevel();
		float	pitch = newLevel > 30 ? 1.0F : static_cast<float>(newLevel) / 30.0F;
		level.playSoundAt(nullptr, player.getX(), player.getY(), player.getZ(), "minecraft:entity.player.levelup", Level::SoundSource::Players, pitch,
						  1.0F);
	}
} // namespace

namespace Xp {
	// The points needed to go from `level` to `level + 1` (XpBar.getXpNeededForNextLevel)
	int xpNeededForNextLevel(int level) {
		if (level >= 30) return 112 + (level - 30) * 9;
		if (level >= 15) return 37 + (level - 15) * 5;
		return 7 + level * 2;
	}

	// The total points accumulated at the start of `level` (XpBar.getTotalExperienceForLevel)
	int xpToLevel(int level) {
		if (level <= 0) return 0;
		if (level <= 16) return level * level + 6 * level;
		if (level <= 31) return static_cast<int>(2.5 * level * level - 40.5 * level + 360);
		return static_cast<int>(4.5 * level * level - 162.5 * level + 2220);
	}

	void addExperience(Server& server, Player& player, int amount) {
		if (player.getGameMode() == GameMode::Spectator || amount == 0) return;
		int total = std::max(0, player.getXpTotal() + amount);
		player.setXpTotal(total);
		// The level is where the total sits, and the progress is the fraction of the way to the next
		int oldLevel = player.getXpLevel();
		int level	 = 0;
		while (xpToLevel(level + 1) <= total) level++;
		player.setXpLevel(level);
		int	  needed	  = xpNeededForNextLevel(level);
		float progress	  = static_cast<float>(total - xpToLevel(level));
		player.setXpProgress(needed > 0 ? progress / static_cast<float>(needed) : 0.0F);
		if (level > oldLevel) levelUpSound(server, player, level);
		send(server, player);
	}

	void addLevels(Server& server, Player& player, int levels) {
		if (player.getGameMode() == GameMode::Spectator || levels == 0) return;
		int	 oldLevel = player.getXpLevel();
		float fraction = std::clamp(player.getXpProgress(), 0.0F, 1.0F);
		int	 level	= std::max(0, player.getXpLevel() + levels);
		player.setXpLevel(level);
		player.setXpProgress(fraction);
		player.setXpTotal(xpToLevel(level) + static_cast<int>(static_cast<float>(xpNeededForNextLevel(level)) * fraction));
		if (level > oldLevel) levelUpSound(server, player, level);
		send(server, player);
	}

	void send(Server& server, Player& player) {
		Buffer packet;
		packet.writeVarInt(player.getXpLevel());
		packet.writeFloat(player.getXpProgress());
		packet.writeVarInt(player.getXpTotal());
		Packet::send(player.shared_from_this(), PacketId::Play::Clientbound::SET_EXPERIENCE, packet, server);
	}

	int blockXp(const GameData& gameData, int blockId) {
		const std::string& name = gameData.getStaticName("minecraft:block", blockId);
		if (name == "minecraft:coal_ore" || name == "minecraft:deepslate_coal_ore") return 2;
		if (name == "minecraft:diamond_ore" || name == "minecraft:deepslate_diamond_ore") return 7;
		if (name == "minecraft:emerald_ore" || name == "minecraft:deepslate_emerald_ore") return 7;
		if (name == "minecraft:lapis_ore" || name == "minecraft:deepslate_lapis_ore") return 5;
		if (name == "minecraft:nether_quartz_ore") return 5;
		if (name == "minecraft:nether_gold_ore") return 1;
		if (name == "minecraft:redstone_ore" || name == "minecraft:deepslate_redstone_ore") return 5;
		return 0;
	}

	int mobXp(const GameData& gameData, int typeId) {
		const std::string& name = gameData.getStaticName("minecraft:entity_type", typeId);
		if (name == "minecraft:cow" || name == "minecraft:pig" || name == "minecraft:chicken" || name == "minecraft:sheep") return 3;
		if (name == "minecraft:zombie" || name == "minecraft:husk" || name == "minecraft:creeper" || name == "minecraft:spider" ||
			name == "minecraft:cave_spider" || name == "minecraft:skeleton") return 5;
		if (name == "minecraft:wither") return 50;
		if (name == "minecraft:ender_dragon") return 12000;
		return 0;
	}
} // namespace Xp