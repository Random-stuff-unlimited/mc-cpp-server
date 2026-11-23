#include "world/entity/mobs/Critters.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/ItemEntity.hpp"
#include "world/entity/ai/Goals.hpp"
#include "world/entity/ai/RandomPos.hpp"
#include "world/item/ItemUse.hpp"

#include <cmath>
#include <stdexcept>

namespace {
	constexpr int SAVE_VERSION				 = 1;
	constexpr int SERIALIZER_SNIFFER_STATE	 = 30;
	constexpr int SERIALIZER_ARMADILLO_STATE = 31;

	bool isItem(Level& level, const ItemStack& stack, const char* name) {
		return !stack.isEmpty() && stack.item == level.gameData().getStaticId("minecraft:item", name);
	}
	ItemStack itemOf(Level& level, const char* name, int count = 1) { return ItemStack(level.gameData().getStaticId("minecraft:item", name), count); }
	bool	  hurtActor(Level& level, Actor& target, const Combat::DamageSource& source, float amount) {
		 if (Player* player = target.asPlayer()) return Combat::damage(level.server(), *player, amount, source);
		 return target.hurtServer(source, amount);
	}

	// ----- Goat -----

	// The goat's ram (GoatAi: PrepareRamNearestTarget then RamTarget): once its cooldown is over, it picks the nearest
	// living thing it sees 4 to 16 blocks away (not a goat, not a creative player), turns to it and paws the ground for a
	// second, then charges in a straight line; what it runs into is hurt and thrown far away
	class GoatRamGoal : public Goal {
	  public:
		explicit GoatRamGoal(Goat& goat) : _goat(goat) {
			setFlags({Flag::Move, Flag::Look, Flag::Jump});
		}
		bool canUse() override {
			if (_cooldown < 0) _cooldown = _goat.nextRamCooldown(); // Once it spawned (screaming or not)
			if (_cooldown > 0) {
				_cooldown -= 2; // Ticked every other tick while it doesn't run
				return false;
			}
			if (!_goat.onGround() || _goat.isInWaterNow() || _goat.isPanicking() || _goat.hasRider()) return false;
			TargetingConditions conditions = TargetingConditions::forCombat().withRange(16.0).withSelector([this](Actor& actor, Level&) {
				const GameData::EntityTypeInfo* type = _goat.level().gameData().getEntityType(actor.typeId());
				if (type && type->is("Goat")) return false;
				double d = actor.distanceToSqr(_goat);
				return d >= 16.0 && notCreativeOrSpectator(actor);
			});
			std::vector<Actor*> candidates = actorsOfClass(_goat.level(), "LivingEntity", _goat.boundingBox().inflate(16.0));
			for (Actor* player : _goat.level().actorsIn(_goat.boundingBox().inflate(16.0))) {
				if (player->asPlayer()) candidates.push_back(player);
			}
			Actor* target = nearestActor(_goat.level(), candidates, conditions, &_goat, _goat.position().x, _goat.position().y, _goat.position().z);
			if (!target) {
				_cooldown = 20; // Look again in a second
				return false;
			}
			_target = EntityRef(*target);
			return true;
		}
		bool canContinueToUse() override { return _phase != Phase::Done; }
		void start() override {
			_phase = Phase::Preparing;
			_ticks = 0;
			_goat.navigation().stop();
			_goat.playSound(_goat.isScreamingGoat() ? "minecraft:entity.goat.screaming.prepare_ram" : "minecraft:entity.goat.prepare_ram", 1.0F, 1.0F);
		}
		void stop() override {
			_goat.navigation().stop();
			_cooldown = _goat.nextRamCooldown();
			_phase	  = Phase::Done;
			_target.clear();
		}
		bool requiresUpdateEveryTick() override { return true; }
		void tick() override {
			Actor* target = _target.isSet() ? _goat.level().actorByRef(_target) : nullptr;
			_ticks++;
			if (_phase == Phase::Preparing) {
				if (!target || !target->isAlive()) {
					_phase = Phase::Done;
					return;
				}
				_goat.lookControl().setLookAt(*target, 30.0F, 30.0F);
				if (_ticks >= 20) {
					// The charge: toward where the target stands now, a bit beyond it
					Vec3 to		= target->position() - _goat.position();
					_direction	= Vec3{to.x, 0.0, to.z}.normalize();
					Vec3 end	= _goat.position() + _direction.scale(std::sqrt(to.horizontalDistanceSqr()) + 2.0);
					_end		= end;
					_phase		= Phase::Charging;
					_ticks		= 0;
				}
				return;
			}
			// Charging
			_goat.moveControl().setWantedPosition(_end.x, _end.y, _end.z, 3.0); // RAM speed (GoatAi: 3.0, 2.5 for babies)
			for (Actor* hit : _goat.level().actorsIn(_goat.boundingBox(), &_goat)) {
				if (!hit->isAlive() || (!hit->asLiving() && !hit->asPlayer())) continue;
				const GameData::EntityTypeInfo* type = _goat.level().gameData().getEntityType(hit->typeId());
				if (type && type->is("Goat")) continue;
				ram(*hit);
				_phase = Phase::Done;
				return;
			}
			Vec3 left = _end - _goat.position();
			if (_ticks > 40 || left.horizontalDistanceSqr() < 0.25 || _goat.horizontalCollision()) _phase = Phase::Done;
		}

	  private:
		enum class Phase { Preparing, Charging, Done };
		Goat&	  _goat;
		EntityRef _target;
		Phase	  _phase	= Phase::Done;
		int		  _cooldown = -1, _ticks = 0;
		Vec3	  _direction, _end;

		void ram(Actor& target) {
			float damage = static_cast<float>(_goat.getAttributeValue(_goat.attributeIds().attackDamage));
			hurtActor(_goat.level(), target, {"minecraft:mob_attack", &_goat, nullptr, std::nullopt}, damage);
			// RamTarget: its speed (and speed effects) make the knockback; babies push less
			float speedBonus = 0.0F;
			if (const MobEffects::Instance* speed = _goat.effects().get(MobEffects::Speed)) speedBonus += 0.25F * (speed->amplifier + 1);
			if (const MobEffects::Instance* slow = _goat.effects().get(MobEffects::Slowness)) speedBonus -= 0.25F * (slow->amplifier + 1);
			float strength = Mth::clamp(_goat.speed() * 1.65F, 0.2F, 3.0F) + speedBonus;
			strength *= _goat.isBaby() ? 1.0F : 2.5F;
			if (LivingEntity* living = target.asLiving()) {
				living->knockback(strength, -_direction.x, -_direction.z);
			} else {
				target.pushMotion({_direction.x * strength, std::min(0.4, static_cast<double>(strength)), _direction.z * strength});
			}
			_goat.playSound(_goat.isScreamingGoat() ? "minecraft:entity.goat.screaming.ram_impact" : "minecraft:entity.goat.ram_impact", 1.0F, 1.0F);
		}
	};

	// ----- Armadillo -----

	// While rolled up (or rolling, unrolling) it doesn't move nor look around: every control is taken
	class ArmadilloRolledUpGoal : public Goal {
	  public:
		explicit ArmadilloRolledUpGoal(Armadillo& armadillo) : _armadillo(armadillo) { setFlags({Flag::Move, Flag::Look, Flag::Jump}); }
		bool canUse() override { return _armadillo.isScared(); }
		void start() override { _armadillo.navigation().stop(); }

	  private:
		Armadillo& _armadillo;
	};

	// ----- Sniffer -----

	// The sniffer's search (SnifferAi: Scenting, then Sniffing toward a diggable block, Digging, Rising)
	class SnifferDigGoal : public Goal {
	  public:
		explicit SnifferDigGoal(Sniffer& sniffer) : _sniffer(sniffer) { setFlags({Flag::Move, Flag::Look, Flag::Jump}); }
		bool canUse() override {
			if (!_sniffer.canSniff() || !_sniffer.onGround() || _sniffer.isInWaterNow() || _sniffer.isPanicking()) return false;
			return _sniffer.random().nextInt(reducedTickDelay(400)) == 0;
		}
		bool canContinueToUse() override { return _phase != Phase::Done && !_sniffer.isPanicking(); }
		void start() override {
			_phase = Phase::Scenting;
			_ticks = 0;
			_sniffer.navigation().stop();
			_sniffer.setState(Sniffer::State::Scenting);
			_sniffer.playSound("minecraft:entity.sniffer.scenting", 1.0F, 1.0F);
		}
		void stop() override {
			_sniffer.setState(Sniffer::State::Idling);
			_sniffer.startSniffCooldown();
			_phase = Phase::Done;
		}
		bool requiresUpdateEveryTick() override { return true; }
		void tick() override {
			_ticks++;
			switch (_phase) {
				case Phase::Scenting:
					if (_ticks >= 40) {
						// A diggable block within 10 blocks to walk to
						std::optional<BlockPos> found;
						for (int i = 0; i < 10 && !found; i++) {
							std::optional<Vec3> pos = RandomPos::defaultPos(_sniffer, 10, 7);
							if (pos) {
								BlockPos block{Mth::floor(pos->x), Mth::floor(pos->y), Mth::floor(pos->z)};
								if (_sniffer.canDig(block)) found = block;
							}
						}
						if (!found && _sniffer.canDig(_sniffer.blockPosition())) found = _sniffer.blockPosition();
						if (!found) {
							_phase = Phase::Done;
							return;
						}
						_digPos = *found;
						_sniffer.setState(Sniffer::State::Sniffing);
						_sniffer.playSound("minecraft:entity.sniffer.sniffing", 1.0F, 1.0F);
						_sniffer.navigation().moveTo(_digPos.x + 0.5, _digPos.y, _digPos.z + 0.5, 1.25);
						_phase = Phase::Sniffing;
						_ticks = 0;
					}
					break;
				case Phase::Sniffing:
					if (_sniffer.navigation().isDone() || _ticks > 400) {
						if (!_sniffer.canDig(_sniffer.blockPosition())) {
							_phase = Phase::Done;
							return;
						}
						_sniffer.navigation().stop();
						_sniffer.setState(Sniffer::State::Digging);
						_sniffer.setDropSeedAtTick(_sniffer.tickCount() + 120);
						_phase = Phase::Digging;
						_ticks = 0;
					}
					break;
				case Phase::Digging:
					if (_ticks % 20 == 0) _sniffer.playSound("minecraft:entity.sniffer.digging", 1.0F, 1.0F);
					if (_ticks >= 120) {
						_sniffer.dropSeed(_sniffer.blockPosition());
						_sniffer.setState(Sniffer::State::Rising);
						_phase = Phase::Rising;
						_ticks = 0;
					}
					break;
				case Phase::Rising:
					if (_ticks >= 30) _phase = Phase::Done;
					break;
				case Phase::Done: break;
			}
		}

	  private:
		enum class Phase { Scenting, Sniffing, Digging, Rising, Done };
		Sniffer& _sniffer;
		Phase	 _phase = Phase::Done;
		int		 _ticks = 0;
		BlockPos _digPos;
	};
} // namespace

// ===================== Goat =====================

void Goat::registerGoals() {
	_goalSelector.addGoal(0, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(1, std::make_unique<PanicGoal>(*this, 2.0));
	_goalSelector.addGoal(2, std::make_unique<BreedGoal>(*this, 1.0));
	_goalSelector.addGoal(3, std::make_unique<TemptGoal>(*this, 1.25, [this](const ItemStack& stack) { return isFood(stack); }, false));
	_goalSelector.addGoal(4, std::make_unique<GoatRamGoal>(*this));
	_goalSelector.addGoal(5, std::make_unique<FollowParentGoal>(*this, 1.25));
	_goalSelector.addGoal(6, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 1.0));
	_goalSelector.addGoal(7, std::make_unique<LookAtPlayerGoal>(*this, "Player", 6.0F));
	_goalSelector.addGoal(8, std::make_unique<RandomLookAroundGoal>(*this));
}

void Goat::setScreamingGoat(bool screaming) {
	if (screaming == _screaming) return;
	_screaming = screaming;
	markData(DATA_IS_SCREAMING_GOAT);
}

int Goat::nextRamCooldown() {
	// UniformInt.of(600, 6000), screaming goats UniformInt.of(100, 300)
	return _screaming ? 100 + _random.nextInt(201) : 600 + _random.nextInt(5401);
}

std::string Goat::ambientSoundName() { return _screaming ? "minecraft:entity.goat.screaming.ambient" : "minecraft:entity.goat.ambient"; }

bool Goat::mobInteract(Player& player, int hand) {
	const ItemStack& stack = player.getStackInHand(hand);
	if (isItem(_level, stack, "minecraft:bucket") && !isBaby()) {
		_level.playSoundAt(&player, player.getX(), player.getY(), player.getZ(),
						   _screaming ? "minecraft:entity.goat.screaming.milk" : "minecraft:entity.goat.milk", Level::SoundSource::Players, 1.0F, 1.0F);
		ItemStack held = ItemUse::filledResult(_level, player, stack, itemOf(_level, "minecraft:milk_bucket"));
		player.inventory().set(player.handSlot(hand), std::move(held));
		return true;
	}
	return Animal::mobInteract(player, hand);
}

std::unique_ptr<Mob> Goat::getBreedOffspring(Animal& partner) {
	std::unique_ptr<Mob> baby  = Animal::getBreedOffspring(partner);
	auto*				 child = dynamic_cast<Goat*>(baby.get());
	auto*				 other = dynamic_cast<Goat*>(&partner);
	// Goat.getBreedOffspring: a screamer if either parent is one (a coin toss)
	if (child && other) child->setScreamingGoat(_random.nextBoolean() ? _screaming : other->_screaming);
	return baby;
}

std::shared_ptr<SpawnGroupData> Goat::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	setScreamingGoat(_level.random().nextDouble() < 0.02);
	return Animal::finalizeSpawn(difficulty, reason, group);
}

void Goat::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Animal::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_IS_SCREAMING_GOAT, onlyNonDefault, !_screaming)) writeBoolData(buf, DATA_IS_SCREAMING_GOAT, _screaming);
	if (wantsData(mask, DATA_HAS_LEFT_HORN, onlyNonDefault, _leftHorn)) writeBoolData(buf, DATA_HAS_LEFT_HORN, _leftHorn);
	if (wantsData(mask, DATA_HAS_RIGHT_HORN, onlyNonDefault, _rightHorn)) writeBoolData(buf, DATA_HAS_RIGHT_HORN, _rightHorn);
}

void Goat::save(Buffer& buf) const {
	Animal::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeBool(_screaming);
	buf.writeBool(_leftHorn);
	buf.writeBool(_rightHorn);
}

void Goat::load(Buffer& buf) {
	Animal::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown goat data version");
	setScreamingGoat(buf.readBool());
	_leftHorn  = buf.readBool();
	_rightHorn = buf.readBool();
}

// ===================== Armadillo =====================

Armadillo::Armadillo(Level& level, int typeId) : Animal(level, typeId) { _scuteTime = pickNextScuteDropTime(); }

void Armadillo::registerGoals() {
	_goalSelector.addGoal(0, std::make_unique<ArmadilloRolledUpGoal>(*this));
	_goalSelector.addGoal(1, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(2, std::make_unique<PanicGoal>(*this, 2.0));
	_goalSelector.addGoal(3, std::make_unique<BreedGoal>(*this, 1.0));
	_goalSelector.addGoal(4, std::make_unique<TemptGoal>(*this, 1.25, [this](const ItemStack& stack) { return isFood(stack); }, false));
	_goalSelector.addGoal(5, std::make_unique<FollowParentGoal>(*this, 1.25));
	_goalSelector.addGoal(6, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 1.0));
	_goalSelector.addGoal(7, std::make_unique<LookAtPlayerGoal>(*this, "Player", 6.0F));
	_goalSelector.addGoal(8, std::make_unique<RandomLookAroundGoal>(*this));
}

int Armadillo::pickNextScuteDropTime() { return _random.nextInt(20 * 60 * 5) + 20 * 60 * 5; }

void Armadillo::setState(State state) {
	if (state == _state) return;
	_state		  = state;
	_inStateTicks = 0;
	markData(DATA_STATE);
}

void Armadillo::rollUp() {
	if (isScared()) return;
	navigation().stop();
	_safeTicks = 0;
	setState(State::Rolling);
	playSound("minecraft:entity.armadillo.roll", 1.0F, 1.0F);
}

void Armadillo::rollOut() {
	if (!isScared()) return;
	setState(State::Unrolling);
	playSound("minecraft:entity.armadillo.unroll_start", 1.0F, 1.0F);
}

bool Armadillo::isScaredBy(Actor& actor) {
	if (&actor == this || !actor.isAlive()) return false;
	if (_level.gameData().isInTag("minecraft:entity_type", "minecraft:undead", actor.typeId())) return true;
	Player* player = actor.asPlayer();
	return player && notCreativeOrSpectator(actor) && (player->isSprinting() || player->vehicleId() != -1);
}

bool Armadillo::hurtServer(const Combat::DamageSource& source, float amount) {
	// Rolled up, its shell takes half of what is left after the first point
	if (isScared()) amount = (amount - 1.0F) / 2.0F;
	bool hurt = Animal::hurtServer(source, amount);
	if (hurt && isAlive() && !isBaby()) rollUp();
	return hurt;
}

void Armadillo::customServerAiStep() {
	Animal::customServerAiStep();
	_inStateTicks++;
	bool threat = false;
	if (_tickCount % 10 == 0 || isScared()) {
		for (Actor* actor : _level.actorsIn(boundingBox().inflate(7.0), this)) {
			if (isScaredBy(*actor)) {
				threat = true;
				break;
			}
		}
	}
	switch (_state) {
		case State::Idle:
			if (threat && !isBaby()) rollUp();
			break;
		case State::Rolling:
			if (_inStateTicks >= ROLLING_TICKS) setState(State::Scared);
			break;
		case State::Scared:
			// It peeks out after a while without threat, then unrolls
			_safeTicks = threat ? 0 : _safeTicks + 1;
			if (_safeTicks >= SCARE_CHECK_INTERVAL) rollOut();
			break;
		case State::Unrolling:
			if (threat) {
				setState(State::Scared);
			} else if (_inStateTicks >= UNROLLING_TICKS) {
				setState(State::Idle);
				playSound("minecraft:entity.armadillo.unroll_finish", 1.0F, 1.0F);
			}
			break;
	}
	// A scute now and then
	if (isAlive() && !isBaby() && --_scuteTime <= 0) {
		playSound("minecraft:entity.armadillo.scute_drop", 1.0F, (_random.nextFloat() - _random.nextFloat()) * 0.2F + 1.0F);
		spawnAtLocation(itemOf(_level, "minecraft:armadillo_scute"));
		_scuteTime = pickNextScuteDropTime();
	}
}

void Armadillo::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Animal::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_STATE, onlyNonDefault, _state == State::Idle)) {
		buf.writeUByte(static_cast<uint8_t>(DATA_STATE));
		buf.writeVarInt(SERIALIZER_ARMADILLO_STATE);
		buf.writeVarInt(static_cast<int>(_state));
	}
}

void Armadillo::save(Buffer& buf) const {
	Animal::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeUByte(static_cast<uint8_t>(_state));
	buf.writeInt(_scuteTime);
}

void Armadillo::load(Buffer& buf) {
	Animal::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown armadillo data version");
	setState(static_cast<State>(buf.readUByte() & 3));
	_scuteTime = buf.readInt();
}

// ===================== Sniffer =====================

Sniffer::Sniffer(Level& level, int typeId) : Animal(level, typeId) {
	setPathfindingMalus(PathType::Water, -1.0F);
	setPathfindingMalus(PathType::DangerFire, -1.0F);
	setPathfindingMalus(PathType::DamageFire, -1.0F);
}

void Sniffer::registerGoals() {
	_goalSelector.addGoal(0, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(1, std::make_unique<PanicGoal>(*this, 2.0));
	_goalSelector.addGoal(2, std::make_unique<BreedGoal>(*this, 1.0));
	_goalSelector.addGoal(3, std::make_unique<TemptGoal>(*this, 1.25, [this](const ItemStack& stack) { return isFood(stack); }, false));
	_goalSelector.addGoal(4, std::make_unique<SnifferDigGoal>(*this));
	_goalSelector.addGoal(5, std::make_unique<FollowParentGoal>(*this, 1.25));
	_goalSelector.addGoal(6, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 1.0));
	_goalSelector.addGoal(7, std::make_unique<LookAtPlayerGoal>(*this, "Player", 6.0F));
	_goalSelector.addGoal(8, std::make_unique<RandomLookAroundGoal>(*this));
}

void Sniffer::setState(State state) {
	if (state == _state) return;
	_state = state;
	markData(DATA_STATE);
}

void Sniffer::setDropSeedAtTick(int tick) {
	_dropSeedAtTick = tick;
	markData(DATA_DROP_SEED_AT_TICK);
}

bool Sniffer::canDig(const BlockPos& pos) {
	// Sniffer.canDig: a diggable block under it, air above, not in water
	BlockPos below = pos.below();
	int		 block = _level.blocks().blockOf(_level.getBlockState(below));
	return _level.gameData().isInTag("minecraft:block", "minecraft:sniffer_diggable_block", block) && _level.blocks().isAir(_level.getBlockState(pos));
}

void Sniffer::dropSeed(const BlockPos& pos) {
	if (!canDig(pos)) return;
	// The gameplay/sniffer_digging loot table: one of the two ancient seeds
	ItemStack seed = itemOf(_level, _random.nextBoolean() ? "minecraft:torchflower_seeds" : "minecraft:pitcher_pod");
	if (ItemEntity* item = spawnAtLocation(std::move(seed))) item->setDeltaMovement({0.0, 0.2, 0.0});
	playSound("minecraft:entity.sniffer.drop_seed", 1.0F, 1.0F);
}

void Sniffer::spawnChildFromBreeding(Animal& partner) {
	// Sniffer.spawnChildFromBreeding: an egg instead of a baby
	playSound("minecraft:block.sniffer_egg.plop", 1.0F, (_random.nextFloat() - _random.nextFloat()) * 0.2F + 0.5F);
	spawnAtLocation(itemOf(_level, "minecraft:sniffer_egg"));
	setAge(PARENT_AGE_AFTER_BREEDING);
	partner.setAge(PARENT_AGE_AFTER_BREEDING);
	resetLove();
	partner.resetLove();
	broadcastEntityEvent(EVENT_IN_LOVE_HEARTS);
}

void Sniffer::customServerAiStep() {
	Animal::customServerAiStep();
	if (_sniffCooldown > 0) _sniffCooldown--;
}

void Sniffer::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Animal::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_STATE, onlyNonDefault, _state == State::Idling)) {
		buf.writeUByte(static_cast<uint8_t>(DATA_STATE));
		buf.writeVarInt(SERIALIZER_SNIFFER_STATE);
		buf.writeVarInt(static_cast<int>(_state));
	}
	if (wantsData(mask, DATA_DROP_SEED_AT_TICK, onlyNonDefault, _dropSeedAtTick == 0)) writeIntData(buf, DATA_DROP_SEED_AT_TICK, _dropSeedAtTick);
}

void Sniffer::save(Buffer& buf) const {
	Animal::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeInt(_sniffCooldown);
}

void Sniffer::load(Buffer& buf) {
	Animal::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown sniffer data version");
	_sniffCooldown = buf.readInt();
}
