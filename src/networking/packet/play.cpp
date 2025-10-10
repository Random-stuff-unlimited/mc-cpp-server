#include "buffer.hpp"
#include "nbt.hpp"
#include "networking.hpp"
#include "packet.hpp"
#include "server.hpp"

#include <iostream>
#include <memory>
#include <sstream>

void writePlayPacket(Packet& packet, Server& server) {
	Player* player = packet.getPlayer();
	if (!player) return;

	Buffer buf;

	// --- 1. Entity ID ---
	buf.writeInt(player->getPlayerID());

	// --- 2. Is Hardcore ---
	buf.writeBool(false);

	// --- 3. Game mode ---
	buf.writeByte(0); // Survival

	// --- 4. Previous game mode ---
	buf.writeByte(-1);

	// --- 5. World names ---
	buf.writeVarInt(1);
	buf.writeString("minecraft:overworld");

	// --- 6. Registry codec (NBT) ---
	nbt::TagCompound root;

	// Minimal dimension_type
	nbt::TagCompound dimensionTypes;
	nbt::TagCompound overworldType;
	overworldType["piglin_safe"]          = true;
	overworldType["natural"]              = true;
	overworldType["ambient_light"]        = 0.0f;
	overworldType["logical_height"]       = 256;
	overworldType["infiniburn"]           = "minecraft:infiniburn_overworld";
	overworldType["ultrawarm"]            = false;
	overworldType["coordinate_scale"]     = 1.0f;
	overworldType["has_skylight"]         = true;
	overworldType["has_ceiling"]          = false;
	overworldType["respawn_anchor_works"] = false;
	overworldType["bed_works"]            = true;
	overworldType["effects"]              = "minecraft:overworld";
	dimensionTypes["minecraft:overworld"] = std::make_shared<nbt::TagCompound>(overworldType);

	root["minecraft:dimension_type"] = std::make_shared<nbt::TagCompound>(dimensionTypes);

	// Minimal biome registry
	nbt::TagCompound biomes;
	nbt::TagCompound plainsBiome;
	plainsBiome["precipitation"] = std::string("rain");
	plainsBiome["depth"]         = 0.125f;
	plainsBiome["temperature"]   = 0.8f;
	plainsBiome["scale"]         = 0.05f;
	plainsBiome["downfall"]      = 0.4f;
	plainsBiome["category"]      = std::string("plains");
	biomes["minecraft:plains"]   = std::make_shared<nbt::TagCompound>(plainsBiome);

	root["minecraft:worldgen/biome"] = std::make_shared<nbt::TagCompound>(biomes);

	// Minimal noise settings registry
	nbt::TagCompound noiseSettings;
	nbt::TagCompound overworldNoise;
	overworldNoise["sea_level"]          = 63;
	overworldNoise["height"]             = "minecraft:overworld";
	noiseSettings["minecraft:overworld"] = std::make_shared<nbt::TagCompound>(overworldNoise);

	root["minecraft:worldgen/noise_settings"] = std::make_shared<nbt::TagCompound>(noiseSettings);

	// Encode NBT into buffer
	std::ostringstream nbtStream;
	nbt::NBT nbtData("root", root);
	nbtData.encode(nbtStream);
	std::string nbtBytes = nbtStream.str();
	buf.writeBytes(std::vector<uint8_t>(nbtBytes.begin(), nbtBytes.end()));

	// --- 7. Dimension (current) ---
	buf.writeString("minecraft:overworld");

	// --- 8. Hashed seed ---
	buf.writeLong(12345678L);

	// --- 9. Max players ---
	buf.writeVarInt(20);

	// --- 10. View distance ---
	buf.writeVarInt(10);

	// --- 11. Simulation distance ---
	buf.writeVarInt(10);

	// --- 12. Reduced debug info ---
	buf.writeBool(false);

	// --- 13. Enable respawn screen ---
	buf.writeBool(true);

	// --- 14. Do limited crafting ---
	buf.writeBool(false);

	// --- 15. Dimension type name ---
	buf.writeString("minecraft:overworld");

	// --- 16. Dimension name ---
	buf.writeString("minecraft:overworld");

	// --- 17. Portal cooldown ---
	buf.writeVarInt(0);

	// --- 18. Enforces secure chat ---
	buf.writeBool(false);

	// --- Encapsulate packet ID + data ---
	int packetId = 0x2B; // Login packet
	Buffer payload;
	payload.writeVarInt(packetId);
	payload.writeBytes(buf.getData());

	// --- Final buffer with size prefix ---
	Buffer finalBuf;
	finalBuf.writeVarInt(payload.getData().size());
	finalBuf.writeBytes(payload.getData());

	packet.getData() = finalBuf;
	packet.setPacketSize(finalBuf.getData().size());
	packet.setPacketId(packetId);
	packet.setReturnPacket(PACKET_SEND);
}
