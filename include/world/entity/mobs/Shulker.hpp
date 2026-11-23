#ifndef SHULKER_HPP
#define SHULKER_HPP

#include "world/entity/Projectile.hpp"
#include "world/entity/mobs/Monster.hpp"

// Shulker: a box stuck to a block face. It peeks out now and then, opens wide to shoot homing bullets (levitation)
// at the players within 20 blocks, takes no arrows while closed (and 20 more armor), and teleports away when its
// block goes or, badly hurt, one time in four. Duplicating when hit by its own bullets isn't ported
class Shulker : public Monster {
  public:
	static constexpr int DATA_ATTACH_FACE = 16, DATA_PEEK = 17, DATA_COLOR = 18;
	static constexpr int NO_COLOR = 16;

	Shulker(Level& level, int typeId);

	Direction attachFace() const { return _attachFace; }
	void	  setAttachFace(Direction face);
	int		  rawPeekAmount() const { return _peek; }
	void	  setRawPeekAmount(int peek);
	bool	  isClosed() const { return _peek == 0; }
	int		  color() const { return _color; }
	void	  setColor(int color);
	// Shulker.teleportSomewhere: to an empty spot within 8 blocks with a face to stick to
	bool	  teleportSomewhere();

	void							registerGoals() override;
	void							tick() override;
	bool							hurtServer(const Combat::DamageSource& source, float amount) override;
	bool							isPushable() const override { return false; }
	bool							onClimbable() override { return false; }
	int								maxHeadYRot() const override { return 180; }
	int								maxHeadXRot() const override { return 180; }
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	std::string						ambientSoundName() override { return "minecraft:entity.shulker.ambient"; }
	void							save(Buffer& buf) const override;
	void							load(Buffer& buf) override;
	using Monster::playSound;

  protected:
	void travel(const Vec3&) override {} // It never moves on its own
	void checkFallDamage(double, bool, const BlockPos&) override {}
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	Direction _attachFace = Direction::Down;
	int		  _peek = 0, _color = NO_COLOR;

	// Shulker.findAttachableSurface: a sturdy face of a neighbor it can stick to, from pos
	std::optional<Direction> findAttachableSurface(const BlockPos& pos);
	bool					 canStayAt(const BlockPos& pos, Direction face);
};

// ShulkerBullet: flies toward its target, turning to follow it; what it hits takes 4 damage and levitates for 10
// seconds. One hit destroys it
class ShulkerBullet : public Projectile {
  public:
	ShulkerBullet(Level& level, Shulker* shulker, Actor* target);
	explicit ShulkerBullet(Level& level) : ShulkerBullet(level, nullptr, nullptr) {}

	void tick() override;
	bool hurtServer(const Combat::DamageSource& source, float amount) override;

  private:
	EntityRef _target;
	int		  _life = 0;

	void onHit(Actor* actor);
};

#endif
