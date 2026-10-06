#ifndef PANDA_HPP
#define PANDA_HPP

#include "world/entity/mobs/Animal.hpp"

class PandaLookAtPlayerGoal;

// Panda: two genes (a main and a hidden one, recessive ones show only when both match) make it normal, lazy, worried,
// playful, brown, weak or aggressive. Sits to eat the bamboo and cake it finds, rolls, lies on its back, sneezes;
// breeds only with bamboo around; bites back once (aggressive ones keep fighting)
class Panda : public Animal {
  public:
	static constexpr int DATA_UNHAPPY_COUNTER = 17, DATA_SNEEZE_COUNTER = 18, DATA_EAT_COUNTER = 19, DATA_MAIN_GENE = 20, DATA_HIDDEN_GENE = 21, DATA_FLAGS = 22;
	static constexpr int FLAG_SNEEZE = 2, FLAG_ROLL = 4, FLAG_SIT = 8, FLAG_ON_BACK = 16;
	enum Gene { Normal = 0, Lazy = 1, Worried = 2, Playful = 3, Brown = 4, Weak = 5, Aggressive = 6 };

	Panda(Level& level, int typeId);

	int	 unhappyCounter() const { return _unhappyCounter; }
	void setUnhappyCounter(int value);
	bool isSneezing() const { return _pandaFlags & FLAG_SNEEZE; }
	void sneeze(bool value);
	bool isSitting() const { return _pandaFlags & FLAG_SIT; }
	void sit(bool value) { setFlag(FLAG_SIT, value); }
	bool isOnBack() const { return _pandaFlags & FLAG_ON_BACK; }
	void setOnBack(bool value) { setFlag(FLAG_ON_BACK, value); }
	bool isRolling() const { return _pandaFlags & FLAG_ROLL; }
	void roll(bool value) { setFlag(FLAG_ROLL, value); }
	bool isEating() const { return _eatCounter > 0; }
	void eat(bool value) { setEatCounter(value ? 1 : 0); }
	void setMainGene(int gene);
	void setHiddenGene(int gene);
	// Gene.getVariantFromGenes
	int	 variant() const;
	bool isLazy() const { return variant() == Lazy; }
	bool isWorried() const { return variant() == Worried; }
	bool isPlayful() const { return variant() == Playful; }
	bool isWeak() const { return variant() == Weak; }
	bool isAggressiveGene() const { return variant() == Aggressive; }
	bool isScared() const;
	bool canPerformAction() const { return !isOnBack() && !isScared() && !isEating() && !isRolling() && !isSitting(); }
	void tryToSit();
	void setAttributes();
	static bool canPickUpAndEat(Level& level, ItemEntity& item);
	ItemEntity* dropHeld(const ItemStack& stack) { return spawnAtLocation(stack); }

	// What its goals share
	bool					gotBamboo = false, didBite = false;
	PandaLookAtPlayerGoal* lookAtPlayerGoal = nullptr;

	void							registerGoals() override;
	void							tick() override;
	bool							doHurtTarget(Actor& target) override;
	void							playAttackSound() override { playSound("minecraft:entity.panda.bite", 1.0F, 1.0F); }
	bool							hurtServer(const Combat::DamageSource& source, float amount) override;
	bool							mobInteract(Player& player, int hand) override;
	bool							isFood(const ItemStack& stack) const override;
	std::unique_ptr<Mob>			getBreedOffspring(Animal& partner) override;
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	void							save(Buffer& buf) const override;
	void							load(Buffer& buf) override;

  protected:
	void		pickUpItem(ItemEntity& item) override;
	float		babySpawnChance() const override { return 0.2F; }
	std::string ambientSoundName() override;
	void		writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	int		_unhappyCounter = 0, _sneezeCounter = 0, _eatCounter = 0, _rollCounter = 0;
	uint8_t _mainGene = 0, _hiddenGene = 0, _pandaFlags = 0;
	Vec3	_rollDelta{};

	void	   setFlag(int flag, bool value);
	void	   setEatCounter(int value);
	void	   setSneezeCounter(int value);
	void	   handleEating();
	void	   handleRoll();
	void	   afterSneeze();
	int		   oneOfGenesRandomly() { return _random.nextBoolean() ? _mainGene : _hiddenGene; }
	static int randomGene(JavaRandom& random);
};

#endif
