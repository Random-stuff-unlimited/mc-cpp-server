#ifndef PRIMED_TNT_HPP
#define PRIMED_TNT_HPP

#include "world/entity/Entity.hpp"

// A lit TNT (vanilla's PrimedTnt): falls, bounces a little, and explodes (power 4) when its fuse (80 ticks) runs out
class PrimedTnt : public Entity {
  public:
	static constexpr int DEFAULT_FUSE = 80;

	// Vanilla's PrimedTnt(level, x, y, z, owner): a small random push, the fuse lit
	PrimedTnt(Level& level, const Vec3& position, Actor* owner);

	int	   fuse() const { return _fuse; }
	void   setFuse(int fuse);
	Actor* owner() const override;

	void tick() override;
	bool shouldBeSaved() const override { return true; }
	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;
	void writeEntityData(Buffer& buf) const override;
	void writeDirtyEntityData(Buffer& buf) const override { writeEntityData(buf); }

  private:
	int		  _fuse			  = DEFAULT_FUSE;
	float	  _explosionPower = 4.0F;
	EntityRef _owner;
	bool	  _usedPortal = false;

	void explode();
};

#endif
