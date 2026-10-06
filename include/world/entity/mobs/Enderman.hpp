#ifndef ENDERMAN_HPP
#define ENDERMAN_HPP

#include "world/entity/ai/NeutralAnger.hpp"
#include "world/entity/mobs/Monster.hpp"

class Player;

// EnderMan: neutral until a player stares at it (looks at its eyes without a carved pumpkin on) or hurts it. It then
// freezes while stared at, teleports behind or toward its target, and runs faster. Projectiles never hit it (it
// teleports away), water and rain hurt it, daylight calms it down and sends it away. It picks blocks up and puts them
// down, and attacks endermites
class Enderman : public Monster {
  public:
	static constexpr int DATA_CARRY_STATE = 16, DATA_CREEPY = 17, DATA_STARED_AT = 18;

	Enderman(Level& level, int typeId);

	bool isCreepy() const { return _creepy; }
	bool hasBeenStaredAt() const { return _staredAt; }
	void setBeingStaredAt();
	// The block state it carries, -1 if none
	int	 carriedBlock() const { return _carried; }
	void setCarriedBlock(int state);
	NeutralAnger& anger() { return _anger; }
	// EnderMan.isBeingStaredBy: the player looks at its eyes, with nothing in between
	bool isBeingStaredBy(Player& player);
	// EnderMan.teleport: to a random spot within 32 blocks; teleportTowards: 16 blocks toward an entity
	bool teleport();
	bool teleportTowards(Actor& target);

	void  registerGoals() override;
	void  setTarget(Actor* target) override;
	bool  isSensitiveToWater() const override { return true; }
	float getWalkTargetValue(const BlockPos&) override { return 0.0F; }
	bool  hurtServer(const Combat::DamageSource& source, float amount) override;
	void  checkDespawn() override;
	void  save(Buffer& buf) const override;
	void  load(Buffer& buf) override;

  protected:
	void		aiStep() override;
	void		customServerAiStep() override;
	std::string ambientSoundName() override { return _creepy ? "minecraft:entity.enderman.scream" : "minecraft:entity.enderman.ambient"; }
	void		dropAllDeathLoot(const Combat::DamageSource& source) override;
	void		writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	int			 _carried = -1;
	bool		 _creepy = false, _staredAt = false;
	int			 _targetChangeTime = 0;
	NeutralAnger _anger;

	bool teleportTo(double x, double y, double z);
};

#endif
