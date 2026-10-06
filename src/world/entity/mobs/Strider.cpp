#include "world/entity/mobs/Strider.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/entity/ai/Goals.hpp"

namespace {
	bool isLavaBlock(Level& level, const BlockPos& pos) { return level.fluids().isLava(level.fluids().stateOf(level.getBlockState(pos)).type); }

	// Strider.StriderGoToLavaGoal: out of the lava, to lava within 8 blocks with room above
	class StriderGoToLavaGoal : public MoveToBlockGoal {
	  public:
		StriderGoToLavaGoal(Strider& strider, double speed) : MoveToBlockGoal(strider, speed, 8, 2), _strider(strider) {}
		bool canContinueToUse() override { return !_strider.isInLavaNow() && isValidTarget(_strider.level(), _blockPos); }
		bool canUse() override { return !_strider.isInLavaNow() && MoveToBlockGoal::canUse(); }

	  protected:
		BlockPos getMoveToTarget() const override { return _blockPos; }
		bool	 shouldRecalculatePath() const override { return _tryTicks % 20 == 0; }
		bool	 isValidTarget(Level& level, const BlockPos& pos) override {
			const GameData& data = level.gameData();
			return data.getBlocks().blockOf(level.getBlockState(pos)) == data.getStaticId("minecraft:block", "minecraft:lava") &&
				   (data.getStateProperties(level.getBlockState(pos.above())).pathfindable & static_cast<int>(PathComputationType::Land));
		}

	  private:
		Strider& _strider;
	};

	// Strider.StriderPathNavigation: lava is a destination like any ground
	class StriderPathNavigation : public GroundPathNavigation {
	  public:
		using GroundPathNavigation::GroundPathNavigation;
		bool isStableDestination(const BlockPos& pos) override { return isLavaBlock(_level, pos) || GroundPathNavigation::isStableDestination(pos); }
	};
} // namespace

Strider::Strider(Level& level, int typeId) : Animal(level, typeId) {
	setPathfindingMalus(PathType::Water, -1.0F);
	setPathfindingMalus(PathType::Lava, 0.0F);
	setPathfindingMalus(PathType::DangerFire, 0.0F);
	setPathfindingMalus(PathType::DamageFire, 0.0F);
}

std::unique_ptr<PathNavigation> Strider::createNavigation() { return std::make_unique<StriderPathNavigation>(*this, _level); }

void Strider::registerGoals() {
	_goalSelector.addGoal(1, std::make_unique<PanicGoal>(*this, 1.65));
	_goalSelector.addGoal(2, std::make_unique<BreedGoal>(*this, 1.0));
	auto tempt = std::make_unique<TemptGoal>(
			*this, 1.4, [this](const ItemStack& stack) { return !stack.isEmpty() && _level.gameData().isInTag("minecraft:item", "minecraft:strider_tempt_items", stack.item); },
			false);
	_temptGoal = tempt.get();
	_goalSelector.addGoal(3, std::move(tempt));
	_goalSelector.addGoal(4, std::make_unique<StriderGoToLavaGoal>(*this, 1.0));
	_goalSelector.addGoal(5, std::make_unique<FollowParentGoal>(*this, 1.0));
	_goalSelector.addGoal(7, std::make_unique<RandomStrollGoal>(*this, 1.0, 60));
	_goalSelector.addGoal(8, std::make_unique<LookAtPlayerGoal>(*this, "Player", 8.0F));
	_goalSelector.addGoal(8, std::make_unique<RandomLookAroundGoal>(*this));
	_goalSelector.addGoal(9, std::make_unique<LookAtPlayerGoal>(*this, "Strider", 8.0F));
}

bool Strider::isControlledByRider() {
	Player* player = rider();
	if (!player || !isSaddled()) return false;
	int stick = _level.gameData().getStaticId("minecraft:item", "minecraft:warped_fungus_on_a_stick");
	return player->getStackInHand(0).item == stick || player->getStackInHand(1).item == stick;
}

bool Strider::canStandOnFluid(const FluidState& fluid) const { return _level.fluids().isLava(fluid.type); }

void Strider::setSuffocating(bool suffocating) {
	if (suffocating != _suffocating) {
		_suffocating = suffocating;
		markData(DATA_SUFFOCATING);
	}
	AttributeInstance* speed = _attributes.getInstance(_ids.movementSpeed);
	if (!speed) return;
	if (!suffocating) {
		speed->removeModifier("minecraft:suffocating");
	} else if (!speed->hasModifier("minecraft:suffocating")) {
		speed->addModifier({"minecraft:suffocating", -0.34F, AttributeModifier::Operation::AddMultipliedBase}, false);
	}
}

void Strider::tick() {
	bool tempted = _temptGoal && _temptGoal->isRunning();
	if (tempted && _random.nextInt(140) == 0) {
		playSound("minecraft:entity.strider.happy", soundVolume(), voicePitch());
	} else if (isPanicking() && _random.nextInt(60) == 0) {
		playSound("minecraft:entity.strider.retreat", soundVolume(), voicePitch());
	}
	if (!isNoAi()) {
		const GameData& data = _level.gameData();
		auto			warm = [&](const BlockPos& pos) {
			   return data.isInTag("minecraft:block", "minecraft:strider_warm_blocks", data.getBlocks().blockOf(_level.getBlockState(pos)));
		};
		// getBlockStateOnLegacy: the block 0.2 under its feet
		BlockPos on{Mth::floor(_position.x), Mth::floor(_position.y - 0.2F), Mth::floor(_position.z)};
		setSuffocating(!(warm(blockPosition()) || warm(on) || _lavaHeight > 0.0));
	}
	Animal::tick();
	if (!isRemoved()) floatStrider();
}

void Strider::floatStrider() {
	if (!isInLava()) return;
	BlockPos pos = blockPosition();
	if (_position.y > pos.y + 0.5 - 1.0E-5F && !isLavaBlock(_level, pos.above())) {
		_onGround = true;
	} else {
		_delta = _delta.scale(0.5) + Vec3{0.0, 0.05, 0.0};
	}
}

void Strider::checkFallDamage(double dy, bool onGround, const BlockPos& onPos) {
	if (isInLava()) {
		_fallDistance = 0;
	} else {
		Animal::checkFallDamage(dy, onGround, onPos);
	}
}

std::string Strider::ambientSoundName() {
	return !isPanicking() && !(_temptGoal && _temptGoal->isRunning()) ? "minecraft:entity.strider.ambient" : "";
}

float Strider::getWalkTargetValue(const BlockPos& pos) {
	if (isLavaBlock(_level, pos)) return 10.0F;
	return isInLava() ? -std::numeric_limits<float>::infinity() : 0.0F;
}

bool Strider::isFood(const ItemStack& stack) const {
	return !stack.isEmpty() && _level.gameData().isInTag("minecraft:item", "minecraft:strider_food", stack.item);
}

bool Strider::mobInteract(Player& player, int hand) {
	bool food = isFood(player.getStackInHand(hand));
	if (!food && isSaddled() && !hasRider() && !player.isShiftKeyDown()) {
		startRiding(player);
		return true;
	}
	bool consumed = Animal::mobInteract(player, hand);
	if (!consumed) return isAlive() && !isBaby() && equipFromPlayer(player, hand, EquipmentSlot::Saddle);
	if (consumed && food) {
		_level.playSoundAt(nullptr, _position.x, _position.y, _position.z, "minecraft:entity.strider.eat", Level::SoundSource::Neutral, 1.0F,
						   1.0F + (_random.nextFloat() - _random.nextFloat()) * 0.2F);
	}
	return consumed;
}

std::shared_ptr<SpawnGroupData> Strider::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	if (isBaby()) return Animal::finalizeSpawn(difficulty, reason, group);
	JavaRandom& random = _level.random();
	if (random.nextInt(30) == 0) {
		// A zombified piglin jockey: without its rider (no passengers yet), it keeps the saddle
		setItemSlot(EquipmentSlot::Saddle, ItemStack(_level.gameData().getStaticId("minecraft:item", "minecraft:saddle"), 1));
		setGuaranteedDrop(EquipmentSlot::Saddle);
		group = std::make_shared<AgeableMobGroupData>(true, 0.0F);
	} else if (random.nextInt(10) == 0) {
		group = std::make_shared<AgeableMobGroupData>(true, 0.0F); // A baby rider: not ported
	} else {
		group = std::make_shared<AgeableMobGroupData>(true, 0.5F);
	}
	return Animal::finalizeSpawn(difficulty, reason, group);
}

void Strider::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Animal::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_BOOST_TIME, onlyNonDefault, true)) writeIntData(buf, DATA_BOOST_TIME, 0);
	if (wantsData(mask, DATA_SUFFOCATING, onlyNonDefault, !_suffocating)) writeBoolData(buf, DATA_SUFFOCATING, _suffocating);
}

void Strider::save(Buffer& buf) const { Animal::save(buf); }

void Strider::load(Buffer& buf) {
	Animal::load(buf);
	_dirtyData = 0;
}
