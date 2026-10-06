#include "world/entity/mobs/Enderman.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Clip.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/Throwables.hpp"
#include "world/entity/ai/Goals.hpp"
#include "world/item/Components.hpp"
#include "world/item/Enchantments.hpp"
#include "world/item/LootTables.hpp"

#include <cmath>

namespace {
	constexpr int		  SAVE_VERSION = 1;
	constexpr int		  SERIALIZER_OPTIONAL_BLOCK_STATE = 15;
	constexpr const char* SPEED_MODIFIER_ATTACKING		  = "minecraft:attacking";

	// EnderMan.EndermanFreezeWhenLookedAt: stands still while its target stares at it within 16 blocks
	class FreezeWhenLookedAtGoal : public Goal {
	  public:
		explicit FreezeWhenLookedAtGoal(Enderman& enderman) : _enderman(enderman) { setFlags({Flag::Jump, Flag::Move}); }
		bool canUse() override {
			Actor*	target = _enderman.getTarget();
			Player* player = target ? target->asPlayer() : nullptr;
			if (!player || player->distanceToSqr(_enderman) > 256.0) return false;
			return _enderman.isBeingStaredBy(*player);
		}
		void start() override { _enderman.navigation().stop(); }
		void tick() override {
			if (Actor* target = _enderman.getTarget()) _enderman.lookControl().setLookAt(target->position().x, target->eyeY(), target->position().z);
		}

	  private:
		Enderman& _enderman;
	};

	// EnderMan.EndermanLookForPlayerGoal: a player that stares at it (or it is angry at) becomes its target after 5
	// ticks; then it teleports away when stared at from up close, toward its target when it is far
	class LookForPlayerGoal : public NearestAttackableTargetGoal {
	  public:
		explicit LookForPlayerGoal(Enderman& enderman)
			: NearestAttackableTargetGoal(enderman, "Player", 10, false, false, [&enderman](Actor& target, Level&) { return enderman.anger().isAngryAt(target); }),
			  _enderman(enderman) {}
		bool canUse() override {
			TargetingConditions conditions = TargetingConditions::forCombat();
			conditions.withRange(getFollowDistance()).withSelector([this](Actor& target, Level&) { return isAngerInducing(target); });
			const Vec3& p	   = _enderman.position();
			Actor*		player = nearestPlayer(_enderman.level(), conditions, &_enderman, p.x, p.y, p.z);
			if (!player) return false;
			_pending = EntityRef(*player);
			return true;
		}
		void start() override {
			_aggroTime	  = adjustedTickDelay(5);
			_teleportTime = 0;
			_enderman.setBeingStaredAt();
		}
		void stop() override {
			_pending.clear();
			NearestAttackableTargetGoal::stop();
		}
		bool canContinueToUse() override {
			if (_pending.isSet()) {
				Actor* pending = _enderman.level().actorByRef(_pending);
				if (!pending || !isAngerInducing(*pending)) return false;
				_enderman.lookAt(*pending, 10.0F, 10.0F);
				return true;
			}
			if (Actor* target = _target.isSet() ? _enderman.level().actorByRef(_target) : nullptr) {
				TargetingConditions conditions = TargetingConditions::forCombat();
				conditions.ignoreLineOfSight();
				if (conditions.test(_enderman.level(), &_enderman, *target)) return true;
			}
			return NearestAttackableTargetGoal::canContinueToUse();
		}
		void tick() override {
			if (!_enderman.getTarget()) setTarget(nullptr);
			if (_pending.isSet()) {
				if (--_aggroTime <= 0) {
					_target = _pending;
					_pending.clear();
					NearestAttackableTargetGoal::start();
				}
				return;
			}
			Actor*	target = _target.isSet() ? _enderman.level().actorByRef(_target) : nullptr;
			Player* player = target ? target->asPlayer() : nullptr;
			if (!player) return;
			if (_enderman.isBeingStaredBy(*player)) {
				if (player->distanceToSqr(_enderman) < 16.0) _enderman.teleport();
				_teleportTime = 0;
			} else if (player->distanceToSqr(_enderman) > 256.0 && _teleportTime++ >= adjustedTickDelay(30) && _enderman.teleportTowards(*player)) {
				_teleportTime = 0;
			}
		}

	  private:
		Enderman& _enderman;
		EntityRef _pending;
		int		  _aggroTime = 0, _teleportTime = 0;

		bool isAngerInducing(Actor& target) {
			Player* player = target.asPlayer();
			return (player && _enderman.isBeingStaredBy(*player)) || _enderman.anger().isAngryAt(target);
		}
	};

	// EnderMan.EndermanTakeBlockGoal: now and then takes a holdable block within reach that it can see
	class TakeBlockGoal : public Goal {
	  public:
		explicit TakeBlockGoal(Enderman& enderman) : _enderman(enderman) {}
		bool canUse() override { return _enderman.carriedBlock() < 0 && _enderman.random().nextInt(reducedTickDelay(20)) == 0; }
		void tick() override {
			JavaRandom& random = _enderman.random();
			Level&		level  = _enderman.level();
			const Vec3& p	   = _enderman.position();
			int			x	   = Mth::floor(p.x - 2.0 + random.nextDouble() * 4.0);
			int			y	   = Mth::floor(p.y + random.nextDouble() * 3.0);
			int			z	   = Mth::floor(p.z - 2.0 + random.nextDouble() * 4.0);
			BlockPos	pos{x, y, z};
			int			state = level.getBlockState(pos);
			BlockPos	at	  = _enderman.blockPosition();
			Clip::HitResult hit = Clip::clip(level, {at.x + 0.5, y + 0.5, at.z + 0.5}, {x + 0.5, y + 0.5, z + 0.5}, Clip::BlockMode::Outline, Clip::FluidMode::None);
			if (level.gameData().isInTag("minecraft:block", "minecraft:enderman_holdable", level.blocks().blockOf(state)) && hit.hit && hit.pos == pos) {
				level.setBlock(pos, level.gameData().getDefaultBlockState("minecraft:air"), Level::UPDATE_ALL); // removeBlock
				_enderman.setCarriedBlock(level.blocks().defaultState(level.blocks().blockOf(state)));
			}
		}

	  private:
		Enderman& _enderman;
	};

	// EnderMan.EndermanLeaveBlockGoal: rarely puts its block down on a full block next to it
	class LeaveBlockGoal : public Goal {
	  public:
		explicit LeaveBlockGoal(Enderman& enderman) : _enderman(enderman) {}
		bool canUse() override { return _enderman.carriedBlock() >= 0 && _enderman.random().nextInt(reducedTickDelay(2000)) == 0; }
		void tick() override {
			JavaRandom& random = _enderman.random();
			Level&		level  = _enderman.level();
			const Vec3& p	   = _enderman.position();
			BlockPos	pos{Mth::floor(p.x - 1.0 + random.nextDouble() * 2.0), Mth::floor(p.y + random.nextDouble() * 2.0),
							Mth::floor(p.z - 1.0 + random.nextDouble() * 2.0)};
			int			carried = _enderman.carriedBlock();
			if (carried < 0) return;
			carried			  = level.updateFromNeighbourShapes(carried, pos);
			int	 below		  = level.getBlockState(pos.below());
			const auto& shape = level.gameData().getCollisionShape(below);
			bool fullBelow	  = shape.size() == 1 && shape[0].minX <= 0.0 && shape[0].minY <= 0.0 && shape[0].minZ <= 0.0 && shape[0].maxX >= 1.0 &&
								shape[0].maxY >= 1.0 && shape[0].maxZ >= 1.0;
			AABB cell{static_cast<double>(pos.x), static_cast<double>(pos.y), static_cast<double>(pos.z), pos.x + 1.0, pos.y + 1.0, pos.z + 1.0};
			bool empty = true;
			level.entities().forEachIn(cell, [&](Entity& entity) { empty &= &entity == &_enderman || entity.isRemoved(); });
			for (const auto& player : level.players()) empty &= !player->boundingBox().intersects(cell);
			if (level.blocks().isAir(level.getBlockState(pos)) && !level.blocks().isAir(below) &&
				level.blocks().blockOf(below) != level.gameData().getStaticId("minecraft:block", "minecraft:bedrock") && fullBelow &&
				level.behavior(carried).canSurvive(level, pos, carried) && empty) {
				level.setBlock(pos, carried, Level::UPDATE_ALL);
				_enderman.setCarriedBlock(-1);
			}
		}

	  private:
		Enderman& _enderman;
	};
} // namespace

Enderman::Enderman(Level& level, int typeId) : Monster(level, typeId), _anger(*this, 20, 39) { setPathfindingMalus(PathType::Water, -1.0F); }

void Enderman::registerGoals() {
	_goalSelector.addGoal(0, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(1, std::make_unique<FreezeWhenLookedAtGoal>(*this));
	_goalSelector.addGoal(2, std::make_unique<MeleeAttackGoal>(*this, 1.0, false));
	_goalSelector.addGoal(7, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 1.0, 0.0F));
	_goalSelector.addGoal(8, std::make_unique<LookAtPlayerGoal>(*this, "Player", 8.0F));
	_goalSelector.addGoal(8, std::make_unique<RandomLookAroundGoal>(*this));
	_goalSelector.addGoal(10, std::make_unique<LeaveBlockGoal>(*this));
	_goalSelector.addGoal(11, std::make_unique<TakeBlockGoal>(*this));
	_targetSelector.addGoal(1, std::make_unique<LookForPlayerGoal>(*this));
	_targetSelector.addGoal(2, std::make_unique<HurtByTargetGoal>(*this));
	_targetSelector.addGoal(3, std::make_unique<NearestAttackableTargetGoal>(*this, "Endermite", 10, true, false));
	// ResetUniversalAngerTargetGoal: the universalAnger game rule is off
}

void Enderman::setTarget(Actor* target) {
	Monster::setTarget(target);
	AttributeInstance* speed = _attributes.getInstance(_ids.movementSpeed);
	if (!target) {
		_targetChangeTime = 0;
		if (_creepy) {
			_creepy = false;
			markData(DATA_CREEPY);
		}
		if (_staredAt) {
			_staredAt = false;
			markData(DATA_STARED_AT);
		}
		if (speed) speed->removeModifier(SPEED_MODIFIER_ATTACKING);
	} else {
		_targetChangeTime = _tickCount;
		if (!_creepy) {
			_creepy = true;
			markData(DATA_CREEPY);
		}
		if (speed && !speed->hasModifier(SPEED_MODIFIER_ATTACKING)) {
			speed->addModifier({SPEED_MODIFIER_ATTACKING, 0.15F, AttributeModifier::Operation::AddValue}, false);
		}
	}
}

void Enderman::setBeingStaredAt() {
	if (_staredAt) return;
	_staredAt = true;
	markData(DATA_STARED_AT);
}

void Enderman::setCarriedBlock(int state) {
	if (state >= 0 && _level.blocks().isAir(state)) state = -1;
	if (state == _carried) return;
	_carried = state;
	markData(DATA_CARRY_STATE);
}

bool Enderman::isBeingStaredBy(Player& player) {
	// PLAYER_NOT_WEARING_DISGUISE_ITEM: a carved pumpkin on its head hides the stare
	const ItemStack& helmet = player.inventory().get(5);
	if (!helmet.isEmpty() && _level.gameData().isInTag("minecraft:item", "minecraft:gaze_disguise_equipment", helmet.item)) return false;
	// LivingEntity.isLookingAtMe(player, 0.025, scaled by the distance, at its eyes)
	const float toRadians = static_cast<float>(M_PI / 180.0);
	float		yaw = player.getYaw() * toRadians, pitch = player.getPitch() * toRadians;
	Vec3		view = Vec3{-Mth::sin(yaw) * Mth::cos(pitch), -Mth::sin(pitch), Mth::cos(yaw) * Mth::cos(pitch)}.normalize();
	Vec3		eyes{player.getX(), player.eyeY(), player.getZ()};
	Vec3		to{_position.x - eyes.x, eyeY() - eyes.y, _position.z - eyes.z};
	double		distance = to.length();
	to					 = to.normalize();
	double dot			 = view.x * to.x + view.y * to.y + view.z * to.z;
	if (!(dot > 1.0 - 0.025 / distance)) return false;
	Vec3 mine{_position.x, eyeY(), _position.z};
	if ((mine - eyes).length() > 128.0) return false;
	return !Clip::clip(_level, eyes, mine, Clip::BlockMode::Collider, Clip::FluidMode::None).hit;
}

void Enderman::aiStep() {
	_jumping = false;
	_anger.update(true);
	Monster::aiStep();
}

void Enderman::customServerAiStep() {
	// Daylight calms it down and sends it away
	if (_level.isBrightOutside() && _tickCount >= _targetChangeTime + 600) {
		float brightness = lightLevelDependentMagicValue();
		if (brightness > 0.5F && _level.canSeeSky(blockPosition()) && _random.nextFloat() * 30.0F < (brightness - 0.4F) * 2.0F) {
			setTarget(nullptr);
			teleport();
		}
	}
	Monster::customServerAiStep();
}

bool Enderman::teleport() {
	if (!isAlive()) return false;
	double x = _position.x + (_random.nextDouble() - 0.5) * 64.0;
	double y = _position.y + (_random.nextInt(64) - 32);
	double z = _position.z + (_random.nextDouble() - 0.5) * 64.0;
	return teleportTo(x, y, z);
}

bool Enderman::teleportTowards(Actor& target) {
	Vec3   away = Vec3{_position.x - target.position().x, _position.y + _height * 0.5 - target.eyeY(), _position.z - target.position().z}.normalize();
	double x	= _position.x + (_random.nextDouble() - 0.5) * 8.0 - away.x * 16.0;
	double y	= _position.y + (_random.nextInt(16) - 8) - away.y * 16.0;
	double z	= _position.z + (_random.nextDouble() - 0.5) * 8.0 - away.z * 16.0;
	return teleportTo(x, y, z);
}

bool Enderman::teleportTo(double x, double y, double z) {
	BlockPos		pos{Mth::floor(x), Mth::floor(y), Mth::floor(z)};
	const GameData& data = _level.gameData();
	while (pos.y > _level.minY() && !data.getStateProperties(_level.getBlockState(pos)).blocksMotion) pos.y--;
	int state = _level.getBlockState(pos);
	if (!data.getStateProperties(state).blocksMotion || _level.fluids().isWater(_level.fluids().stateOf(state).type)) return false;
	Vec3 from = _position;
	if (!randomTeleport(x, y, z, true)) return false;
	navigation().stop();
	_level.playSoundAt(nullptr, from.x, from.y, from.z, "minecraft:entity.enderman.teleport", Level::SoundSource::Hostile, 1.0F, 1.0F);
	playSound("minecraft:entity.enderman.teleport", 1.0F, 1.0F);
	return true;
}

bool Enderman::hurtServer(const Combat::DamageSource& source, float amount) {
	const GameData& data	   = _level.gameData();
	bool			projectile = data.isInTag("minecraft:damage_type", "minecraft:is_projectile", data.getSyncedId("minecraft:damage_type", source.type));
	auto*			potion	   = dynamic_cast<ThrownSplashPotion*>(source.direct);
	if (!projectile && !potion) {
		bool hurt = Monster::hurtServer(source, amount);
		// Hurt by the world (not by a living attacker): it leaves, most of the time
		bool living = source.causing && (source.causing->asLiving() || source.causing->asPlayer());
		if (!living && _random.nextInt(10) != 0) teleport();
		return hurt;
	}
	// A projectile never hits: it teleports away. Only a splash water bottle hurts it
	bool hurt = false;
	if (potion) {
		std::optional<std::vector<uint8_t>> contents = Components::get(potion->item(), data, "minecraft:potion_contents");
		int									id		 = contents ? Components::decodePotion(*contents) : -1;
		if (id >= 0 && data.getStaticName("minecraft:potion", id) == "minecraft:water") hurt = Monster::hurtServer(source, amount);
	}
	for (int i = 0; i < 64; i++) {
		if (teleport()) return true;
	}
	return hurt;
}

void Enderman::checkDespawn() {
	if (_carried >= 0) {
		_noActionTime = 0; // requiresCustomPersistence: one that carries a block stays
		return;
	}
	Monster::checkDespawn();
}

void Enderman::dropAllDeathLoot(const Combat::DamageSource& source) {
	Monster::dropAllDeathLoot(source);
	if (_carried < 0) return;
	// Its block, broken as if with an enchanted diamond axe (silk touch...)
	const GameData& data = _level.gameData();
	ItemStack		axe(data.getStaticId("minecraft:item", "minecraft:diamond_axe"), 1);
	DifficultyInstance difficulty = _level.getCurrentDifficultyAt(blockPosition());
	Enchantments::enchantItemFromProvider(data, axe, "minecraft:enderman_loot_drop", difficulty.getSpecialMultiplier(), _random);
	LootTables::Context context{_level, blockPosition(), _carried, &axe, true, 0.0F};
	for (ItemStack& stack : _level.loot().blockDrops(context)) spawnAtLocation(std::move(stack));
}

void Enderman::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Monster::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_CARRY_STATE, onlyNonDefault, _carried < 0)) {
		buf.writeUByte(DATA_CARRY_STATE);
		buf.writeVarInt(SERIALIZER_OPTIONAL_BLOCK_STATE);
		buf.writeVarInt(_carried < 0 ? 0 : _carried); // 0 (air) for none
	}
	if (wantsData(mask, DATA_CREEPY, onlyNonDefault, !_creepy)) writeBoolData(buf, DATA_CREEPY, _creepy);
	if (wantsData(mask, DATA_STARED_AT, onlyNonDefault, !_staredAt)) writeBoolData(buf, DATA_STARED_AT, _staredAt);
}

void Enderman::save(Buffer& buf) const {
	Monster::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeString(_carried < 0 ? "" : _level.gameData().getBlockStateName(_carried));
	_anger.save(buf);
}

void Enderman::load(Buffer& buf) {
	Monster::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown enderman data version");
	std::string carried = buf.readString();
	_carried			= carried.empty() ? -1 : _level.gameData().getBlockStateFromName(carried);
	_anger.load(buf);
	_dirtyData = 0;
}
