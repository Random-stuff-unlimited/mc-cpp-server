#include "world/entity/mobs/Turtle.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/entity/ExperienceOrb.hpp"
#include "world/entity/ai/Goals.hpp"
#include "world/entity/ai/RandomPos.hpp"

#include <cmath>

namespace {
	constexpr int SAVE_VERSION = 1;
	constexpr int LEVEL_EVENT_DESTROY_BLOCK = 2001;

	bool isWaterBlock(Level& level, const BlockPos& pos) { return level.gameData().getBlocks().blockOf(level.getBlockState(pos)) == level.waterBlock(); }
	Vec3 bottomCenter(const BlockPos& pos) { return {pos.x + 0.5, static_cast<double>(pos.y), pos.z + 0.5}; }

	// Turtle.TurtleMoveControl
	class TurtleMoveControl : public MoveControl {
	  public:
		explicit TurtleMoveControl(Turtle& turtle) : MoveControl(turtle), _turtle(turtle) {}
		void tick() override {
			updateSpeed();
			if (_operation != Operation::MoveTo || _turtle.navigation().isDone()) {
				_turtle.setSpeed(0.0F);
				return;
			}
			double dx = _wantedX - _turtle.position().x, dy = _wantedY - _turtle.position().y, dz = _wantedZ - _turtle.position().z;
			double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
			if (distance < 1.0E-5F) {
				_turtle.setSpeed(0.0F);
				return;
			}
			dy /= distance;
			float yaw = static_cast<float>(Mth::atan2(dz, dx) * 180.0F / (float)M_PI) - 90.0F;
			_turtle.setYRotPublic(rotlerp(_turtle.yRot(), yaw, 90.0F));
			_turtle.setYBodyRot(_turtle.yRot());
			float wanted = static_cast<float>(_speedModifier * _turtle.getAttributeValue(_turtle.attributeIds().movementSpeed));
			_turtle.setSpeed(Mth::lerp(0.125F, _turtle.speed(), wanted));
			_turtle.setDeltaMovement(_turtle.deltaMovement() + Vec3{0.0, _turtle.speed() * dy * 0.1, 0.0});
		}

	  private:
		Turtle& _turtle;
		void	updateSpeed() {
			   if (_turtle.touchingWater()) {
				   _turtle.setDeltaMovement(_turtle.deltaMovement() + Vec3{0.0, 0.005, 0.0});
				   if (!_turtle.homeCloserThan(16.0)) _turtle.setSpeed(std::max(_turtle.speed() / 2.0F, 0.08F));
				   if (_turtle.isBaby()) _turtle.setSpeed(std::max(_turtle.speed() / 3.0F, 0.06F));
			   } else if (_turtle.onGround()) {
				   _turtle.setSpeed(std::max(_turtle.speed() / 2.0F, 0.06F));
			   }
		}
	};

	// Turtle.TurtlePathNavigation: travelling, only water is a destination
	class TurtlePathNavigation : public AmphibiousPathNavigation {
	  public:
		TurtlePathNavigation(Turtle& turtle, Level& level) : AmphibiousPathNavigation(turtle, level), _turtle(turtle) {}
		bool isStableDestination(const BlockPos& pos) override {
			if (_turtle.travelPos) return isWaterBlock(_level, pos);
			return !_level.blocks().isAir(_level.getBlockState(pos.below()));
		}

	  private:
		Turtle& _turtle;
	};

	// Panics toward water within 7 blocks, whether it burns or not
	class TurtlePanicGoal : public PanicGoal {
	  public:
		using PanicGoal::PanicGoal;
		bool canUse() override {
			if (!shouldPanic()) return false;
			if (std::optional<BlockPos> water = lookForWater(7)) {
				_posX = water->x;
				_posY = water->y;
				_posZ = water->z;
				return true;
			}
			return findRandomPosition();
		}
	};

	class TurtleBreedGoal : public BreedGoal {
	  public:
		TurtleBreedGoal(Turtle& turtle, double speed) : BreedGoal(turtle, speed), _turtle(turtle) {}
		bool canUse() override { return BreedGoal::canUse() && !_turtle.hasEgg(); }

	  private:
		Turtle& _turtle;
	};

	// With an egg and within 9 blocks of home: onto sand with air above, digs for 10 seconds, lays 1 to 4 eggs
	class TurtleLayEggGoal : public MoveToBlockGoal {
	  public:
		TurtleLayEggGoal(Turtle& turtle, double speed) : MoveToBlockGoal(turtle, speed, 16), _turtle(turtle) {}
		bool canUse() override { return _turtle.hasEgg() && _turtle.homeCloserThan(9.0) && MoveToBlockGoal::canUse(); }
		bool canContinueToUse() override { return MoveToBlockGoal::canContinueToUse() && _turtle.hasEgg() && _turtle.homeCloserThan(9.0); }
		void tick() override {
			MoveToBlockGoal::tick();
			if (_turtle.touchingWater() || !isReachedTarget()) return;
			if (_turtle.layEggCounter < 1) {
				_turtle.setLayingEgg(true);
			} else if (_turtle.layEggCounter > adjustedTickDelay(200)) {
				Level& level = _turtle.level();
				level.playSound(nullptr, _turtle.blockPosition(), "minecraft:entity.turtle.lay_egg", Level::SoundSource::Blocks, 0.3F,
								0.9F + level.random().nextFloat() * 0.2F);
				const BlockRegistry& blocks = level.blocks();
				int					 egg	= level.gameData().getDefaultBlockState("minecraft:turtle_egg");
				level.setBlock(_blockPos.above(), blocks.withInt(egg, blocks.property("eggs"), _turtle.random().nextInt(4) + 1), Level::UPDATE_ALL);
				_turtle.setHasEgg(false);
				_turtle.setLayingEgg(false);
				_turtle.setInLoveTime(600);
			}
			if (_turtle.isLayingEgg()) _turtle.layEggCounter++;
		}

	  protected:
		bool isValidTarget(Level& level, const BlockPos& pos) override {
			return level.blocks().isAir(level.getBlockState(pos.above())) && _turtle.onSand(pos.above());
		}

	  private:
		Turtle& _turtle;
	};

	// On land (babies always; adults unless they go home or carry an egg): to water within 24 blocks
	class TurtleGoToWaterGoal : public MoveToBlockGoal {
	  public:
		TurtleGoToWaterGoal(Turtle& turtle, double speed) : MoveToBlockGoal(turtle, turtle.isBaby() ? 2.0 : speed, 24), _turtle(turtle) {
			_verticalSearchStart = -1;
		}
		bool canContinueToUse() override { return !_turtle.touchingWater() && _tryTicks <= 1200 && isValidTarget(_turtle.level(), _blockPos); }
		bool canUse() override {
			if (_turtle.isBaby() && !_turtle.touchingWater()) return MoveToBlockGoal::canUse();
			return !_turtle.goingHome && !_turtle.touchingWater() && !_turtle.hasEgg() && MoveToBlockGoal::canUse();
		}

	  protected:
		bool shouldRecalculatePath() const override { return _tryTicks % 160 == 0; }
		bool isValidTarget(Level& level, const BlockPos& pos) override { return isWaterBlock(level, pos); }

	  private:
		Turtle& _turtle;
	};

	// With an egg (or now and then, when far from it): back to within 7 blocks of home
	class TurtleGoHomeGoal : public Goal {
	  public:
		TurtleGoHomeGoal(Turtle& turtle, double speed) : _turtle(turtle), _speedModifier(speed) {}
		bool canUse() override {
			if (_turtle.isBaby()) return false;
			if (_turtle.hasEgg()) return true;
			if (_turtle.random().nextInt(reducedTickDelay(700)) != 0) return false;
			return !_turtle.homeCloserThan(64.0);
		}
		void start() override {
			_turtle.goingHome	 = true;
			_stuck				 = false;
			_closeToHomeTryTicks = 0;
		}
		void stop() override { _turtle.goingHome = false; }
		bool canContinueToUse() override { return !_turtle.homeCloserThan(7.0) && !_stuck && _closeToHomeTryTicks <= adjustedTickDelay(600); }
		void tick() override {
			bool close = _turtle.homeCloserThan(16.0);
			if (close) _closeToHomeTryTicks++;
			if (!_turtle.navigation().isDone()) return;
			Vec3				home = bottomCenter(_turtle.homePos);
			std::optional<Vec3> pos	 = RandomPos::defaultPosTowards(_turtle, 16, 3, home, static_cast<float>(M_PI / 10));
			if (!pos) pos = RandomPos::defaultPosTowards(_turtle, 8, 7, home, static_cast<float>(M_PI / 2));
			if (pos && !close && !isWaterBlock(_turtle.level(), {Mth::floor(pos->x), Mth::floor(pos->y), Mth::floor(pos->z)})) {
				pos = RandomPos::defaultPosTowards(_turtle, 16, 5, home, static_cast<float>(M_PI / 2));
			}
			if (!pos) {
				_stuck = true;
				return;
			}
			_turtle.navigation().moveTo(pos->x, pos->y, pos->z, _speedModifier);
		}

	  private:
		Turtle& _turtle;
		double	_speedModifier;
		bool	_stuck				 = false;
		int		_closeToHomeTryTicks = 0;
	};

	// In the water: toward a point up to 512 blocks away
	class TurtleTravelGoal : public Goal {
	  public:
		TurtleTravelGoal(Turtle& turtle, double speed) : _turtle(turtle), _speedModifier(speed) {}
		bool canUse() override { return !_turtle.goingHome && !_turtle.hasEgg() && _turtle.touchingWater(); }
		void start() override {
			JavaRandom& random = _turtle.random();
			int			x = random.nextInt(1025) - 512, y = random.nextInt(9) - 4, z = random.nextInt(1025) - 512;
			const Vec3& p = _turtle.position();
			if (y + p.y > _turtle.level().seaLevel() - 1) y = 0;
			_turtle.travelPos = BlockPos{Mth::floor(x + p.x), Mth::floor(y + p.y), Mth::floor(z + p.z)};
			_stuck			  = false;
		}
		void tick() override {
			if (!_turtle.travelPos) {
				_stuck = true;
				return;
			}
			if (!_turtle.navigation().isDone()) return;
			Vec3				to	= bottomCenter(*_turtle.travelPos);
			std::optional<Vec3> pos = RandomPos::defaultPosTowards(_turtle, 16, 3, to, static_cast<float>(M_PI / 10));
			if (!pos) pos = RandomPos::defaultPosTowards(_turtle, 8, 7, to, static_cast<float>(M_PI / 2));
			if (pos) {
				// Level.hasChunksAt, 34 blocks around
				Level& level = _turtle.level();
				int	   x = Mth::floor(pos->x), z = Mth::floor(pos->z);
				for (int cx = (x - 34) >> 4; cx <= (x + 34) >> 4 && pos; cx++) {
					for (int cz = (z - 34) >> 4; cz <= (z + 34) >> 4 && pos; cz++) {
						if (!level.loadedChunk(cx, cz)) pos.reset();
					}
				}
			}
			if (!pos) {
				_stuck = true;
				return;
			}
			_turtle.navigation().moveTo(pos->x, pos->y, pos->z, _speedModifier);
		}
		bool canContinueToUse() override {
			return !_turtle.navigation().isDone() && !_stuck && !_turtle.goingHome && !_turtle.isInLove() && !_turtle.hasEgg();
		}
		void stop() override { _turtle.travelPos.reset(); }

	  private:
		Turtle& _turtle;
		double	_speedModifier;
		bool	_stuck = false;
	};

	class TurtleRandomStrollGoal : public RandomStrollGoal {
	  public:
		TurtleRandomStrollGoal(Turtle& turtle, double speed, int interval) : RandomStrollGoal(turtle, speed, interval), _turtle(turtle) {}
		bool canUse() override { return !_turtle.touchingWater() && !_turtle.goingHome && !_turtle.hasEgg() && RandomStrollGoal::canUse(); }

	  private:
		Turtle& _turtle;
	};
} // namespace

Turtle::Turtle(Level& level, int typeId) : Animal(level, typeId) {
	setPathfindingMalus(PathType::Water, 0.0F);
	setPathfindingMalus(PathType::DoorIronClosed, -1.0F);
	setPathfindingMalus(PathType::DoorWoodClosed, -1.0F);
	setPathfindingMalus(PathType::DoorOpen, -1.0F);
	setMoveControl(std::make_unique<TurtleMoveControl>(*this));
}

std::unique_ptr<PathNavigation> Turtle::createNavigation() { return std::make_unique<TurtlePathNavigation>(*this, _level); }

void Turtle::registerGoals() {
	_goalSelector.addGoal(0, std::make_unique<TurtlePanicGoal>(*this, 1.2));
	_goalSelector.addGoal(1, std::make_unique<TurtleBreedGoal>(*this, 1.0));
	_goalSelector.addGoal(1, std::make_unique<TurtleLayEggGoal>(*this, 1.0));
	_goalSelector.addGoal(2, std::make_unique<TemptGoal>(*this, 1.1, [this](const ItemStack& stack) { return isFood(stack); }, false));
	_goalSelector.addGoal(3, std::make_unique<TurtleGoToWaterGoal>(*this, 1.0));
	_goalSelector.addGoal(4, std::make_unique<TurtleGoHomeGoal>(*this, 1.0));
	_goalSelector.addGoal(7, std::make_unique<TurtleTravelGoal>(*this, 1.0));
	_goalSelector.addGoal(8, std::make_unique<LookAtPlayerGoal>(*this, "Player", 8.0F));
	_goalSelector.addGoal(9, std::make_unique<TurtleRandomStrollGoal>(*this, 1.0, 100));
}

std::shared_ptr<SpawnGroupData> Turtle::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	homePos = blockPosition();
	return Animal::finalizeSpawn(difficulty, reason, group);
}

void Turtle::setHasEgg(bool has) {
	if (has == _hasEgg) return;
	_hasEgg = has;
	markData(DATA_HAS_EGG);
}

void Turtle::setLayingEgg(bool laying) {
	layEggCounter = laying ? 1 : 0;
	if (laying == _layingEgg) return;
	_layingEgg = laying;
	markData(DATA_LAYING_EGG);
}

bool Turtle::homeCloserThan(double distance) const {
	double dx = homePos.x + 0.5 - _position.x, dy = homePos.y + 0.5 - _position.y, dz = homePos.z + 0.5 - _position.z;
	return dx * dx + dy * dy + dz * dz < distance * distance;
}

bool Turtle::onSand(const BlockPos& pos) const {
	const GameData& data = _level.gameData();
	return data.isInTag("minecraft:block", "minecraft:sand", data.getBlocks().blockOf(_level.getBlockState(pos.below())));
}

bool Turtle::isFood(const ItemStack& stack) const {
	return !stack.isEmpty() && _level.gameData().isInTag("minecraft:item", "minecraft:turtle_food", stack.item);
}

void Turtle::spawnChildFromBreeding(Animal& partner) {
	setHasEgg(true);
	setAge(PARENT_AGE_AFTER_BREEDING);
	partner.setAge(PARENT_AGE_AFTER_BREEDING);
	resetLove();
	partner.resetLove();
	_level.entities().add(std::make_unique<ExperienceOrb>(_level, _position, _random.nextInt(7) + 1));
}

float Turtle::getWalkTargetValue(const BlockPos& pos) {
	if (!goingHome && _level.fluids().isWater(_level.fluids().stateOf(_level.getBlockState(pos)).type)) return 10.0F;
	return onSand(pos) ? 10.0F : _level.getPathfindingCostFromLightLevels(pos);
}

void Turtle::aiStep() {
	Animal::aiStep();
	if (isAlive() && _layingEgg && layEggCounter >= 1 && layEggCounter % 5 == 0) {
		// Digging: the sand's break particles
		BlockPos pos = blockPosition();
		if (onSand(pos)) _level.levelEvent(nullptr, LEVEL_EVENT_DESTROY_BLOCK, pos, _level.getBlockState(pos.below()));
	}
}

void Turtle::ageBoundaryReached() {
	// getAgeScale: babies are 0.3 of the size
	if (isBaby()) {
		setDimensions(_type.width * 0.3F, _type.height * 0.3F, _type.eyeHeight * 0.3F);
	} else {
		setDimensions(_type.width, _type.height, -1.0F);
		// BuiltInLootTables.TURTLE_GROW: a scute
		if (_tickCount > 0) spawnAtLocation(ItemStack(_level.gameData().getStaticId("minecraft:item", "minecraft:turtle_scute"), 1));
	}
}

void Turtle::travel(const Vec3& input) {
	if (!isInWater()) {
		Animal::travel(input);
		return;
	}
	moveRelative(0.1F, input);
	move(_delta);
	_delta = _delta.scale(0.9);
	if (!getTarget() && (!goingHome || !homeCloserThan(20.0))) _delta.y -= 0.005;
}

std::string Turtle::ambientSoundName() {
	return !isInWater() && _onGround && !isBaby() ? "minecraft:entity.turtle.ambient_land" : Animal::ambientSoundName();
}

void Turtle::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Animal::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_HAS_EGG, onlyNonDefault, !_hasEgg)) writeBoolData(buf, DATA_HAS_EGG, _hasEgg);
	if (wantsData(mask, DATA_LAYING_EGG, onlyNonDefault, !_layingEgg)) writeBoolData(buf, DATA_LAYING_EGG, _layingEgg);
}

void Turtle::save(Buffer& buf) const {
	Animal::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeInt(homePos.x);
	buf.writeInt(homePos.y);
	buf.writeInt(homePos.z);
	buf.writeBool(_hasEgg);
}

void Turtle::load(Buffer& buf) {
	Animal::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown turtle data version");
	homePos.x = buf.readInt();
	homePos.y = buf.readInt();
	homePos.z = buf.readInt();
	_hasEgg	  = buf.readBool();
	_dirtyData = 0;
}
