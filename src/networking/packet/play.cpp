#include "network/networking.hpp"
#include "network/packet.hpp"
#include "player.hpp"
#include "network/server.hpp"
#include "network/buffer.hpp"
#include <vector>
#include <string>
#include <sstream>
#include "lib/nbt.hpp"

#include <sstream>
#include <string>
#include <vector>

void writePlayPacket(Packet& packet, Server& /* server */) {
	Player* player = packet.getPlayer();
	if (!player) return;

	Buffer buf;

	// 1. Entity ID
	buf.writeInt(player->getPlayerID());

	// 2. Is Hardcore (false)
	buf.writeBool(false);

	// 3. Gamemode (0 = survival)
	buf.writeByte(0x00);

	// 4. Previous Gamemode (-1 = none)
	buf.writeByte(0xFF);

	// 5. World Names (array of identifiers)
	std::vector<std::string> worlds = {"minecraft:overworld"};
	buf.writeIdentifierArray(worlds);

	// 6. Dimension Codec (NBT binaire)
	nbt::TagCompound dimensionTypeElement;
	dimensionTypeElement["name"] = nbt::Tag(nbt::TagString("minecraft:overworld"));
	dimensionTypeElement["id"]   = nbt::Tag(nbt::TagInt(0));

	nbt::TagCompound elementData;
	elementData["piglin_safe"]   = nbt::Tag(nbt::TagByte(0));
	elementData["natural"]       = nbt::Tag(nbt::TagByte(1));
	elementData["ambient_light"] = nbt::Tag(nbt::TagFloat(0.0f));
	elementData["infiniburn"]    = nbt::Tag(nbt::TagString("minecraft:infiniburn_overworld"));
	elementData["respawn_anchor_works"] = nbt::Tag(nbt::TagByte(0));
	elementData["has_skylight"]         = nbt::Tag(nbt::TagByte(1));
	elementData["bed_works"]            = nbt::Tag(nbt::TagByte(1));
	elementData["effects"]              = nbt::Tag(nbt::TagString("minecraft:overworld"));
	elementData["has_raids"]            = nbt::Tag(nbt::TagByte(1));
	elementData["logical_height"]       = nbt::Tag(nbt::TagInt(256));
	elementData["coordinate_scale"]     = nbt::Tag(nbt::TagDouble(1.0));
	elementData["ultrawarm"]            = nbt::Tag(nbt::TagByte(0));
	elementData["has_ceiling"]          = nbt::Tag(nbt::TagByte(0));

	dimensionTypeElement["element"] = nbt::Tag(std::make_shared<nbt::TagCompound>(elementData));

	nbt::TagList dimensionTypeList;
	dimensionTypeList.push_back(nbt::Tag(std::make_shared<nbt::TagCompound>(dimensionTypeElement)));

	nbt::TagCompound dimensionType;
	dimensionType["type"]  = nbt::Tag(nbt::TagString("minecraft:dimension_type"));
	dimensionType["value"] = nbt::Tag(std::make_shared<nbt::TagList>(dimensionTypeList));

	nbt::TagCompound codec;
	codec["minecraft:dimension_type"] = nbt::Tag(std::make_shared<nbt::TagCompound>(dimensionType));

	std::stringstream codec_ss;
	codec.encode(codec_ss);
	std::string codec_nbt = codec_ss.str();
	buf.writeNBT(codec_nbt);

	// 7. Dimension (NBT binaire)
	nbt::TagCompound dimension;
	dimension["piglin_safe"]          = nbt::Tag(nbt::TagByte(0));
	dimension["natural"]              = nbt::Tag(nbt::TagByte(1));
	dimension["ambient_light"]        = nbt::Tag(nbt::TagFloat(0.0f));
	dimension["infiniburn"]           = nbt::Tag(nbt::TagString("minecraft:infiniburn_overworld"));
	dimension["respawn_anchor_works"] = nbt::Tag(nbt::TagByte(0));
	dimension["has_skylight"]         = nbt::Tag(nbt::TagByte(1));
	dimension["bed_works"]            = nbt::Tag(nbt::TagByte(1));
	dimension["effects"]              = nbt::Tag(nbt::TagString("minecraft:overworld"));
	dimension["has_raids"]            = nbt::Tag(nbt::TagByte(1));
	dimension["logical_height"]       = nbt::Tag(nbt::TagInt(256));
	dimension["coordinate_scale"]     = nbt::Tag(nbt::TagDouble(1.0));
	dimension["ultrawarm"]            = nbt::Tag(nbt::TagByte(0));
	dimension["has_ceiling"]          = nbt::Tag(nbt::TagByte(0));

	std::stringstream dim_ss;
	dimension.encode(dim_ss);
	std::string dim_nbt = dim_ss.str();
	buf.writeNBT(dim_nbt);

	// 8. World Name
	buf.writeIdentifier("minecraft:overworld");

	// 9. Hashed Seed
	buf.writeLong(0);

	// 10. Max Players (ignored)
	buf.writeVarInt(0);

	// 11. View Distance
	buf.writeVarInt(10);

	// 12. Simulation Distance
	buf.writeVarInt(10);

	// 13. Reduced Debug Info
	buf.writeBool(false);

	// 14. Enable Respawn Screen
	buf.writeBool(false);

	// 15. Is Debug
	buf.writeBool(false);

	// 16. Is Flat
	buf.writeBool(false);

	// 17. Portal cooldown
	buf.writeVarInt(0);

	// 18. Sea level
	buf.writeVarInt(63);

	// 19. Enforce secure chat
	buf.writeBool(false);

	// 20. Has death location
	buf.writeBool(false);

	// 21. Préparer le packet final
	int packetId = 0x2B; // Join Game
	Buffer finalBuf;
	finalBuf.writeVarInt(packetId);
	finalBuf.writeBytes(buf.getData());

	// Taille du packet
	Buffer outBuf;
	outBuf.writeVarInt(finalBuf.getData().size());
	outBuf.writeBytes(finalBuf.getData());

	packet.getData() = outBuf;
	packet.setPacketSize(outBuf.getData().size());
	packet.setReturnPacket(PACKET_SEND);
}
