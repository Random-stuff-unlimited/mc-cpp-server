#ifndef END_GATEWAY_BLOCK_ENTITY_HPP
#define END_GATEWAY_BLOCK_ENTITY_HPP

#include "world/blockentity/BlockEntity.hpp"

#include <optional>

// TheEndGatewayBlockEntity: where it sends what enters it (its exit, found the first time for the gateways the dragon's
// deaths open: an outer island 1024 blocks away, with a gateway back), and its cooldown and age (the beam)
class EndGatewayBlockEntity : public BlockEntity {
  public:
	explicit EndGatewayBlockEntity(const BlockPos& pos) : BlockEntity("minecraft:end_gateway", pos) {}

	const std::optional<BlockPos>& exitPortal() const { return _exit; }
	void						   setExitPosition(const BlockPos& exit, bool exact) {
		  _exit	 = exit;
		  _exact = exact;
		  markChanged();
	}
	bool isExact() const { return _exact; }
	bool isCoolingDown() const { return _cooldown > 0; }
	// triggerCooldown: 2 seconds before it works again, and the beam on the clients (block event 1)
	void triggerCooldown(Level& level);

	bool ticks() const override { return true; }
	void tick(Level& level) override;
	void save(BlockEntityWriter& out) const override;
	void load(BlockEntityReader& in) override;
	void writeUpdateTag(std::vector<uint8_t>& out) const override;
	bool hasUpdatePacket() const override { return true; }

	// EndGatewayFeature: a gateway at pos, framed by bedrock above and below, leading to exit (if set)
	static void place(Level& level, const BlockPos& pos, std::optional<BlockPos> exit, bool exact);

  private:
	std::optional<BlockPos> _exit;
	bool					_exact	  = false;
	int64_t					_age	  = 0;
	int						_cooldown = 0;
};

#endif
