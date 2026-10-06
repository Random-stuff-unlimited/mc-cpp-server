#include "world/entity/mobs/Spider.hpp"

#include "world/Level.hpp"
#include "world/entity/ai/Goals.hpp"

namespace {
	// Spider.SpiderAttackGoal: not while ridden; gives up in the light now and then
	class SpiderAttackGoal : public MeleeAttackGoal {
	  public:
		explicit SpiderAttackGoal(Spider& spider) : MeleeAttackGoal(spider, 1.0, true), _spider(spider) {}
		bool canContinueToUse() override {
			if (_spider.lightLevelDependentMagicValue() >= 0.5F && _spider.random().nextInt(100) == 0) {
				_spider.setTarget(nullptr);
				return false;
			}
			return MeleeAttackGoal::canContinueToUse();
		}

	  private:
		Spider& _spider;
	};

	// Spider.SpiderTargetGoal: only hunts in the dark
	class SpiderTargetGoal : public NearestAttackableTargetGoal {
	  public:
		SpiderTargetGoal(Spider& spider, std::string targetClass) : NearestAttackableTargetGoal(spider, std::move(targetClass), true), _spider(spider) {}
		bool canUse() override { return _spider.lightLevelDependentMagicValue() >= 0.5F ? false : NearestAttackableTargetGoal::canUse(); }

	  private:
		Spider& _spider;
	};

	// Spider.SpiderEffectsGroupData
	struct SpiderEffectsGroupData : SpawnGroupData {
		MobEffects::Effect effect = MobEffects::Count; // Count: none
	};
} // namespace

void Spider::setClimbing(bool climbing) {
	uint8_t flags = static_cast<uint8_t>(climbing ? (_flags | 1) : (_flags & ~1));
	if (flags == _flags) return;
	_flags = flags;
	markData(DATA_FLAGS);
}

void Spider::registerGoals() {
	_goalSelector.addGoal(1, std::make_unique<FloatGoal>(*this));
	// Armadillo.isScared isn't known: every armadillo is avoided only when rolled up in vanilla; none roll here
	_goalSelector.addGoal(2, std::make_unique<AvoidEntityGoal>(*this, "Armadillo", 6.0F, 1.0, 1.2, [](Actor&) { return false; }));
	_goalSelector.addGoal(3, std::make_unique<LeapAtTargetGoal>(*this, 0.4F));
	_goalSelector.addGoal(4, std::make_unique<SpiderAttackGoal>(*this));
	_goalSelector.addGoal(5, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 0.8));
	_goalSelector.addGoal(6, std::make_unique<LookAtPlayerGoal>(*this, "Player", 8.0F));
	_goalSelector.addGoal(6, std::make_unique<RandomLookAroundGoal>(*this));
	_targetSelector.addGoal(1, std::make_unique<HurtByTargetGoal>(*this));
	_targetSelector.addGoal(2, std::make_unique<SpiderTargetGoal>(*this, "Player"));
	_targetSelector.addGoal(3, std::make_unique<SpiderTargetGoal>(*this, "IronGolem"));
}

std::unique_ptr<PathNavigation> Spider::createNavigation() { return std::make_unique<WallClimberNavigation>(*this, _level); }

void Spider::tick() {
	Monster::tick();
	if (!isRemoved()) setClimbing(_horizontalCollision);
}

std::shared_ptr<SpawnGroupData> Spider::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	group			   = Monster::finalizeSpawn(difficulty, reason, group);
	JavaRandom& random = _level.random();
	// Spider jockeys (1%): riding isn't ported, only the draw is made
	if (random.nextInt(100) == 0) {
	}
	if (!group) {
		auto effects = std::make_shared<SpiderEffectsGroupData>();
		if (_level.difficulty() == 3 && random.nextFloat() < 0.1F * difficulty.getSpecialMultiplier()) {
			int n			= random.nextInt(5); // setRandomEffect
			effects->effect = n <= 1 ? MobEffects::Speed : n <= 2 ? MobEffects::Strength : n <= 3 ? MobEffects::Regeneration : MobEffects::Invisibility;
		}
		group = effects;
	}
	// The pack's effect, for good
	if (auto* effects = dynamic_cast<SpiderEffectsGroupData*>(group.get()); effects && effects->effect != MobEffects::Count) {
		addEffect(MobEffects::Instance(effects->effect, MobEffects::INFINITE_DURATION));
	}
	return group;
}

void Spider::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Monster::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_FLAGS, onlyNonDefault, _flags == 0)) writeByteData(buf, DATA_FLAGS, _flags);
}

// ----- CaveSpider -----

bool CaveSpider::doHurtTarget(Actor& target) {
	if (!Spider::doHurtTarget(target)) return false;
	// Poison for 7 (normal) or 15 (hard) seconds
	int seconds = _level.difficulty() == 2 ? 7 : _level.difficulty() == 3 ? 15 : 0;
	if (seconds > 0) addEffectTo(target, MobEffects::Instance(MobEffects::Poison, seconds * 20, 0), this);
	return true;
}
