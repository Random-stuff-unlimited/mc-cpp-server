#ifndef IRON_GOLEM_HPP
#define IRON_GOLEM_HPP

#include "world/entity/Mob.hpp"
#include "world/entity/ai/NeutralAnger.hpp"

// IronGolem: guards its village (walks back to it, strolls among its points of interest), attacks the monsters around
// but creepers and whoever provoked it (for 20 to 39 seconds), with a swing of 7.5 to 21.5 damage that throws its
// victim up. Never drowns nor despawns; an iron ingot repairs it
class IronGolem : public Mob {
  public:
	static constexpr int DATA_FLAGS = 16; // 1: made by a player
	static constexpr int EVENT_ATTACK = 4, EVENT_OFFER_FLOWER = 11, EVENT_STOP_OFFER_FLOWER = 34;

	IronGolem(Level& level, int typeId) : Mob(level, typeId), _anger(*this, 20, 39) {}

	bool isPlayerCreated() const { return _flags & 1; }
	void setPlayerCreated(bool created);
	void offerFlower(bool offer);
	NeutralAnger& anger() { return _anger; }

	void registerGoals() override;
	bool removeWhenFarAway(double) const override { return false; }
	bool canAttackType(int typeId) const override;
	bool doHurtTarget(Actor& target) override;
	bool hurtServer(const Combat::DamageSource& source, float amount) override;
	bool mobInteract(Player& player, int hand) override;
	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;

  protected:
	void aiStep() override;
	void doPush(Entity& other) override;
	bool keepsAirSupply() const override { return true; }
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	uint8_t		 _flags = 0;
	int			 _attackAnimationTick = 0, _offerFlowerTick = 0;
	NeutralAnger _anger;

	// Crackiness.GOLEM: 0 none, 1 low (< 75 %), 2 medium (< 50 %), 3 high (< 25 %)
	int crackiness() const;
};

#endif
