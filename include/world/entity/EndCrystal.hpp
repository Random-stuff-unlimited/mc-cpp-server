#ifndef END_CRYSTAL_HPP
#define END_CRYSTAL_HPP

#include "world/entity/Entity.hpp"

#include <optional>

// EndCrystal: a crystal on obsidian or bedrock that heals the ender dragon nearby (the dragon looks for it). One hit and
// it explodes (power 6); in the End it keeps a fire burning under itself
class EndCrystal : public Entity {
  public:
	static constexpr int DATA_BEAM_TARGET = 8, DATA_SHOW_BOTTOM = 9;

	EndCrystal(Level& level, const Vec3& position);
	explicit EndCrystal(Level& level) : EndCrystal(level, Vec3{}) {}

	bool showsBottom() const { return _showBottom; }
	void setShowBottom(bool show) {
		_showBottom		= show;
		entityDataDirty = true;
	}

	// The beam it shoots (a dragon being summoned, a spike being rebuilt)
	const std::optional<BlockPos>& beamTarget() const { return _beamTarget; }
	void setBeamTarget(std::optional<BlockPos> target) {
		_beamTarget		= target;
		entityDataDirty = true;
	}

	void tick() override;
	bool hurtServer(const Combat::DamageSource& source, float amount) override;
	bool shouldBeSaved() const override { return true; }
	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;
	void writeEntityData(Buffer& buf) const override;

  private:
	bool					_showBottom = true;
	std::optional<BlockPos> _beamTarget;
};

#endif
