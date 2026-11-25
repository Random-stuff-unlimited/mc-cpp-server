#ifndef CRITTERS_HPP
#define CRITTERS_HPP

#include "world/entity/mobs/Animal.hpp"

// Goat: a mountain animal that rams (runs at full speed into) a living thing every 30 seconds to 5 minutes (screaming
// goats: every 5 to 15 seconds), knocking it far away. Milked with a bucket, tempted and bred with wheat. Its long jumps
// and the horns it drops hitting stone aren't ported. The Brain is approximated by goals
class Goat : public Animal {
  public:
	static constexpr int DATA_IS_SCREAMING_GOAT = 17, DATA_HAS_LEFT_HORN = 18, DATA_HAS_RIGHT_HORN = 19;

	Goat(Level& level, int typeId) : Animal(level, typeId) {}

	bool isScreamingGoat() const { return _screaming; }
	void setScreamingGoat(bool screaming);
	// GoatAi.RAM_COOLDOWN_TICKS / TIME_BETWEEN_RAMS_SCREAMER: a new cooldown after a ram
	int	 nextRamCooldown();
	bool hasLeftHorn() const { return _leftHorn; }
	bool hasRightHorn() const { return _rightHorn; }
	// Goat.dropHorn: one of its horns falls off (a goat horn of a random instrument of its kind); false if it has none
	bool dropHorn();

	void							registerGoals() override;
	bool							mobInteract(Player& player, int hand) override;
	std::unique_ptr<Mob>			getBreedOffspring(Animal& partner) override;
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	int								getMaxSpawnClusterSize() const override { return 3; }
	std::string						ambientSoundName() override;
	void							save(Buffer& buf) const override;
	void							load(Buffer& buf) override;
	using Animal::playSound;

  protected:
	float babyEyeHeight() const override { return _type.eyeHeight * 0.5F; }
	void  writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	bool _screaming = false, _leftHorn = true, _rightHorn = true;
};

// Armadillo: rolls up into its shell when hurt or when something scary comes close (a player sprinting or riding, an
// undead mob), taking less damage, and unrolls once it is safe. Drops a scute every 5 to 10 minutes; tempted and bred
// with spider eyes
class Armadillo : public Animal {
  public:
	static constexpr int DATA_STATE = 17;
	// Armadillo.ArmadilloState and its durations
	enum class State : uint8_t { Idle = 0, Rolling = 1, Scared = 2, Unrolling = 3 };
	static constexpr int ROLLING_TICKS = 5, UNROLLING_TICKS = 30, SCARE_CHECK_INTERVAL = 80;

	Armadillo(Level& level, int typeId);

	State state() const { return _state; }
	void  setState(State state);
	bool  isScared() const { return _state != State::Idle; }
	void  rollUp();
	void  rollOut();
	// ArmadilloAi's scare detection: something scary within 7 blocks
	bool  isScaredBy(Actor& actor);

	void		registerGoals() override;
	bool		hurtServer(const Combat::DamageSource& source, float amount) override;
	std::string ambientSoundName() override { return isScared() ? "" : "minecraft:entity.armadillo.ambient"; }
	void		save(Buffer& buf) const override;
	void		load(Buffer& buf) override;
	using Animal::playSound;

  protected:
	void customServerAiStep() override;
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	State _state		= State::Idle;
	int	  _inStateTicks = 0, _scuteTime = 0, _safeTicks = 0;
	bool  _peekReceived = false;

	int pickNextScuteDropTime();
};

// Sniffer: an ancient animal that wanders slowly, and now and then (every 8 minutes at most) scents the ground, walks
// to a diggable block (dirt, grass, moss, mud...), digs it for a few seconds and brings up a torchflower seed or a
// pitcher pod. Bred with torchflower seeds, it lays an egg instead of a baby
class Sniffer : public Animal {
  public:
	static constexpr int DATA_STATE = 17, DATA_DROP_SEED_AT_TICK = 18;
	// Sniffer.State
	enum class State : uint8_t { Idling = 0, FeelingHappy = 1, Scenting = 2, Sniffing = 3, Searching = 4, Digging = 5, Rising = 6 };
	static constexpr int SNIFF_COOLDOWN = 9600;

	Sniffer(Level& level, int typeId);

	State state() const { return _state; }
	void  setState(State state);
	bool  canSniff() const { return !isBaby() && _sniffCooldown <= 0 && _state == State::Idling; }
	void  startSniffCooldown() { _sniffCooldown = SNIFF_COOLDOWN; }
	bool  canDig(const BlockPos& pos);
	// Sniffer.dropSeed: what its digging brings up
	void  dropSeed(const BlockPos& pos);
	void  setDropSeedAtTick(int tick);

	void		registerGoals() override;
	void		spawnChildFromBreeding(Animal& partner) override;
	std::string ambientSoundName() override { return "minecraft:entity.sniffer.idle"; }
	void		save(Buffer& buf) const override;
	void		load(Buffer& buf) override;
	using Animal::playSound;

  protected:
	void customServerAiStep() override;
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	State _state		 = State::Idling;
	int	  _sniffCooldown = 0, _dropSeedAtTick = 0;
};

#endif
