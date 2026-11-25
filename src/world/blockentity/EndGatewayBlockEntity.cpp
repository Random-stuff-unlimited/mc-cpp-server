#include "world/blockentity/EndGatewayBlockEntity.hpp"

#include "data/GameData.hpp"
#include "world/Level.hpp"

#include <cmath>

namespace {
	constexpr uint8_t SAVE_VERSION = 1;
	constexpr int	  COOLDOWN	   = 40;
}

void EndGatewayBlockEntity::triggerCooldown(Level& level) {
	_cooldown = COOLDOWN;
	level.blockEvent(pos(), level.blocks().blockOf(level.getBlockState(pos())), 1, 0);
	markChanged();
}

void EndGatewayBlockEntity::tick(Level& level) {
	_age++;
	if (_cooldown > 0) _cooldown--;
	// The beam every 2400 ticks (beamAnimationTick)
	if (_age % 2400 == 0) triggerCooldown(level);
}

void EndGatewayBlockEntity::save(BlockEntityWriter& out) const {
	out.u8(SAVE_VERSION);
	out.u8(_exit.has_value());
	if (_exit) {
		for (int v : {_exit->x, _exit->y, _exit->z}) out.varint(static_cast<uint32_t>(v));
	}
	out.u8(_exact);
	out.varint(static_cast<uint32_t>(std::min<int64_t>(_age, 0x7FFFFFFF)));
}

void EndGatewayBlockEntity::load(BlockEntityReader& in) {
	std::vector<uint8_t> data = in.rest();
	if (data.empty()) return;
	BlockEntityReader reader(in.gameData, data.data(), data.size());
	reader.u8();
	if (reader.u8()) {
		int x = static_cast<int32_t>(reader.varint()), y = static_cast<int32_t>(reader.varint()), z = static_cast<int32_t>(reader.varint());
		_exit = BlockPos{x, y, z};
	}
	_exact = reader.u8() != 0;
	_age   = reader.varint();
}

// saveCustomOnly: its age (the client draws the spawning beam while young)
void EndGatewayBlockEntity::writeUpdateTag(std::vector<uint8_t>& out) const {
	out.push_back(10);
	out.push_back(4); // Long
	const std::string name = "Age";
	out.push_back(0);
	out.push_back(static_cast<uint8_t>(name.size()));
	out.insert(out.end(), name.begin(), name.end());
	for (int shift = 56; shift >= 0; shift -= 8) out.push_back(static_cast<uint8_t>(_age >> shift));
	out.push_back(0);
}

void EndGatewayBlockEntity::place(Level& level, const BlockPos& origin, std::optional<BlockPos> exit, bool exact) {
	// Its chunks, loaded now (the gateways stand 96 blocks out, the way back 1024)
	for (int dx : {-1, 1}) {
		for (int dz : {-1, 1}) level.loadChunkNow((origin.x + dx) >> 4, (origin.z + dz) >> 4);
	}
	const GameData& data	= level.gameData();
	int				bedrock = data.getDefaultBlockState("minecraft:bedrock"), air = data.getDefaultBlockState("minecraft:air");
	int				gateway = data.getDefaultBlockState("minecraft:end_gateway");
	for (int dx = -1; dx <= 1; dx++) {
		for (int dy = -2; dy <= 2; dy++) {
			for (int dz = -1; dz <= 1; dz++) {
				BlockPos pos = origin.offset(dx, dy, dz);
				bool	 xe = dx == 0, ye = dy == 0, ze = dz == 0, far = std::abs(dy) == 2;
				if (xe && ye && ze) {
					level.setBlock(pos, gateway, Level::UPDATE_ALL);
				} else if (ye) {
					level.setBlock(pos, air, Level::UPDATE_ALL);
				} else if ((far && xe && ze) || ((xe || ze) && !far)) {
					level.setBlock(pos, bedrock, Level::UPDATE_ALL);
				} else {
					level.setBlock(pos, air, Level::UPDATE_ALL);
				}
			}
		}
	}
	if (auto* entity = dynamic_cast<EndGatewayBlockEntity*>(level.getBlockEntity(origin)); entity && exit) entity->setExitPosition(*exit, exact);
}
