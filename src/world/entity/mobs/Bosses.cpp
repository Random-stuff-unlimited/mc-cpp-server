#include "world/entity/AreaEffectCloud.hpp"
#include "world/entity/mobs/Bosses.hpp"

#include "PacketIds.hpp"
#include "network/TextComponent.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Explosion.hpp"
#include "world/Level.hpp"
#include "world/EndDragonFight.hpp"
#include "world/entity/Arrow.hpp"
#include "world/entity/EndCrystal.hpp"
#include "world/entity/ExperienceOrb.hpp"
#include "world/entity/MobRegistry.hpp"
#include "world/entity/ai/Controls.hpp"
#include "world/entity/ai/Goals.hpp"
#include "world/entity/ai/RandomPos.hpp"
#include "world/entity/mobs/FlyingAnimals.hpp"

#include <cmath>
#include <stdexcept>

namespace {
	constexpr int SAVE_VERSION = 1;
	constexpr int LEVEL_EVENT_WITHER_BREAK_BLOCK = 1022, LEVEL_EVENT_WITHER_SPAWNED = 1023, LEVEL_EVENT_WITHER_SHOOT = 1024;
	constexpr int LEVEL_EVENT_DRAGON_SHOOT = 1017;

	bool hurtActor(Level& level, Actor& target, const Combat::DamageSource& source, float amount) {
		if (Player* player = target.asPlayer()) return Combat::damage(level.server(), *player, amount, source);
		return target.hurtServer(source, amount);
	}
	void addEffectTo(Actor& target, const MobEffects::Instance& effect) {
		if (Player* player = target.asPlayer()) {
			player->addEffect(effect);
		} else if (LivingEntity* living = target.asLiving()) {
			living->addEffect(effect);
		}
	}

	// WitherBoss.WitherDoNothingGoal: while it charges up
	class WitherDoNothingGoal : public Goal {
	  public:
		explicit WitherDoNothingGoal(WitherBoss& wither) : _wither(wither) { setFlags({Flag::Move, Flag::Jump, Flag::Look}); }
		bool canUse() override { return _wither.invulnerableTicks() > 0; }

	  private:
		WitherBoss& _wither;
	};

	// WaterAvoidingRandomFlyingGoal, for the wither
	class WitherWanderGoal : public RandomStrollGoal {
	  public:
		explicit WitherWanderGoal(Mob& mob) : RandomStrollGoal(mob, 1.0) {}

	  protected:
		std::optional<Vec3> getPosition() override {
			Vec3				view = viewVectorOf(_mob);
			std::optional<Vec3> pos	 = RandomPos::hoverPos(_mob, 8, 7, view.x, view.z, (float)M_PI / 2.0F, 3, 1);
			return pos ? pos : RandomPos::airAndWaterPos(_mob, 8, 4, -2, view.x, view.z, M_PI / 2.0);
		}
	};
} // namespace

// ===================== BossBar =====================

BossBar::BossBar(Level& level, std::string titleKey, Color color, uint8_t flags, int overlay)
	: _level(level), _uuid(static_cast<uint64_t>(level.random().nextLong()), static_cast<uint64_t>(level.random().nextLong())),
	  _titleKey(std::move(titleKey)), _color(color), _flags(flags), _overlay(overlay) {}

void BossBar::sendAdd(Player& player) {
	Buffer data;
	data.writeUUID(_uuid);
	data.writeVarInt(0); // ADD
	TextComponent::writeTranslatable(data, _titleKey, {});
	data.writeFloat(_progress);
	data.writeVarInt(_color);
	data.writeVarInt(_overlay); // BossBarOverlay
	data.writeUByte(_flags);
	Packet::send(player.shared_from_this(), PacketId::Play::Clientbound::BOSS_EVENT, data, _level.server());
}

void BossBar::sendRemove(Player& player) {
	Buffer data;
	data.writeUUID(_uuid);
	data.writeVarInt(1); // REMOVE
	Packet::send(player.shared_from_this(), PacketId::Play::Clientbound::BOSS_EVENT, data, _level.server());
}

void BossBar::update(const Vec3& center, double range) {
	std::set<int> seen;
	for (const auto& player : _level.players()) {
		if (player->isDisconnected()) continue;
		Vec3 d = player->position() - center;
		if (d.lengthSqr() > range * range) continue;
		seen.insert(player->getPlayerID());
		if (!_players.count(player->getPlayerID())) sendAdd(*player);
	}
	for (int id : _players) {
		if (seen.count(id)) continue;
		if (Actor* actor = _level.actorById(id); actor && actor->asPlayer()) sendRemove(*actor->asPlayer());
	}
	if (std::abs(_progress - _sentProgress) > 1.0E-4F) {
		for (int id : seen) {
			if (!_players.count(id)) continue; // Just added, with its progress
			Actor* actor = _level.actorById(id);
			if (!actor || !actor->asPlayer()) continue;
			Buffer data;
			data.writeUUID(_uuid);
			data.writeVarInt(2); // UPDATE_PROGRESS
			data.writeFloat(_progress);
			Packet::send(actor->asPlayer()->shared_from_this(), PacketId::Play::Clientbound::BOSS_EVENT, data, _level.server());
		}
		_sentProgress = _progress;
	}
	_players = std::move(seen);
}

void BossBar::removeAll() {
	for (int id : _players) {
		if (Actor* actor = _level.actorById(id); actor && actor->asPlayer()) sendRemove(*actor->asPlayer());
	}
	_players.clear();
}

// ===================== WitherBoss =====================

WitherBoss::WitherBoss(Level& level, int typeId) : Monster(level, typeId), _bossBar(level, "entity.minecraft.wither", BossBar::Purple, 1) {
	setMoveControl(std::make_unique<FlyingMoveControl>(*this, 10, false));
	setNoGravity(true);
	_xpReward = 50;
}

WitherBoss::~WitherBoss() = default;

std::unique_ptr<PathNavigation> WitherBoss::createNavigation() {
	auto navigation = std::make_unique<FlyingPathNavigation>(*this, _level);
	navigation->setCanOpenDoors(false);
	navigation->setCanFloat(true);
	return navigation;
}

void WitherBoss::registerGoals() {
	_goalSelector.addGoal(0, std::make_unique<WitherDoNothingGoal>(*this));
	_goalSelector.addGoal(2, std::make_unique<RangedAttackGoal>(*this, 1.0, 40, 20.0F));
	_goalSelector.addGoal(5, std::make_unique<WitherWanderGoal>(*this));
	_goalSelector.addGoal(6, std::make_unique<LookAtPlayerGoal>(*this, "Player", 8.0F));
	_goalSelector.addGoal(7, std::make_unique<RandomLookAroundGoal>(*this));
	_targetSelector.addGoal(1, std::make_unique<HurtByTargetGoal>(*this));
	_targetSelector.addGoal(2, std::make_unique<NearestAttackableTargetGoal>(*this, "LivingEntity", 0, false, false,
																			 [this](Actor& actor, Level&) { return isValidTarget(actor); }));
}

bool WitherBoss::isValidTarget(Actor& actor) {
	if (&actor == this || !actor.isAlive() || (!actor.asLiving() && !actor.asPlayer())) return false;
	if (Player* player = actor.asPlayer(); player && (player->isCreative() || player->isSpectator())) return false;
	return !_level.gameData().isInTag("minecraft:entity_type", "minecraft:wither_friends", actor.typeId());
}

void WitherBoss::setInvulnerableTicks(int ticks) {
	if (ticks == _invulnerableTicks) return;
	_invulnerableTicks = ticks;
	markData(DATA_ID_INV);
}

void WitherBoss::makeInvulnerable() {
	setInvulnerableTicks(INVULNERABLE_TICKS);
	_bossBar.setProgress(0.0F);
	setHealth(maxHealth() / 3.0F);
}

void WitherBoss::setAlternativeTarget(int head, int id) {
	if (_targets[head] == id) return;
	_targets[head] = id;
	markData(DATA_TARGET_A + head);
}

Vec3 WitherBoss::headPosition(int head) const {
	if (head <= 0) return {_position.x, _position.y + 3.0, _position.z};
	float angle = (_yBodyRot + 180 * (head - 1)) * (float)M_PI / 180.0F;
	return {_position.x + Mth::cos(angle) * 1.3, _position.y + 2.2, _position.z + Mth::sin(angle) * 1.3};
}

void WitherBoss::performRangedAttack(Actor& target, float) {
	AABB box = target.boundingBox();
	performHeadAttack(0, {target.position().x, target.position().y + (box.maxY - box.minY) * 0.5, target.position().z}, _random.nextFloat() < 0.001F);
}

void WitherBoss::performHeadAttack(int head, const Vec3& target, bool dangerous) {
	_level.levelEvent(nullptr, LEVEL_EVENT_WITHER_SHOOT, blockPosition(), 0);
	Vec3 from	   = headPosition(head);
	auto skull	   = std::make_unique<WitherSkull>(_level, this, target - from, dangerous);
	skull->setPos(from);
	_level.entities().add(std::move(skull));
}

void WitherBoss::aiStep() {
	// WitherBoss.aiStep: it hovers above its target (5 blocks, until it is powered), keeping 3 blocks away from it
	Vec3   movement = _delta.multiply(1.0, 0.6, 1.0);
	Actor* target	= _targets[0] != 0 ? _level.actorById(_targets[0]) : nullptr;
	if (target && _invulnerableTicks <= 0) {
		double dy = movement.y;
		if (_position.y < target->position().y || (!isPowered() && _position.y < target->position().y + 5.0)) {
			dy = std::max(0.0, dy);
			dy += 0.3 - dy * 0.6;
		}
		movement.y = dy;
		Vec3 flat{target->position().x - _position.x, 0.0, target->position().z - _position.z};
		if (flat.horizontalDistanceSqr() > 9.0) {
			Vec3 to	 = flat.normalize();
			movement = movement + Vec3{to.x * 0.3 - movement.x * 0.6, 0.0, to.z * 0.3 - movement.z * 0.6};
		}
	}
	_delta = movement;
	if (_delta.horizontalDistanceSqr() > 0.05) _yRot = static_cast<float>(Mth::atan2(_delta.z, _delta.x)) * (180.0F / (float)M_PI) - 90.0F;
	Monster::aiStep();
}

void WitherBoss::customServerAiStep() {
	if (_invulnerableTicks > 0) {
		int ticks = _invulnerableTicks - 1;
		_bossBar.setProgress(1.0F - static_cast<float>(ticks) / INVULNERABLE_TICKS);
		if (ticks <= 0) {
			// Charged: it explodes and its howl is heard everywhere
			Explosions::explode(_level, this, std::nullopt, nullptr, {_position.x, eyeY(), _position.z}, 7.0F, false, Explosions::Interaction::Mob);
			_level.levelEvent(nullptr, LEVEL_EVENT_WITHER_SPAWNED, blockPosition(), 0);
		}
		setInvulnerableTicks(ticks);
		if (_tickCount % 10 == 0) heal(10.0F);
		_bossBar.update(_position, 64.0);
		return;
	}
	Monster::customServerAiStep();
	// The side heads: each its own target, shot every 2 to 3 seconds; when idle, a stray skull now and then
	for (int head = 1; head < 3; head++) {
		if (_tickCount < _nextHeadUpdate[head - 1]) continue;
		_nextHeadUpdate[head - 1] = _tickCount + 10 + _random.nextInt(10);
		if (_level.difficulty() >= 2) {
			int& idle = _idleHeadUpdates[head - 1];
			if (idle++ > 15) {
				Vec3 at = _position + Vec3{Mth::nextInt(_random, -10, 10) * 1.0, Mth::nextInt(_random, -5, 5) * 1.0, Mth::nextInt(_random, -10, 10) * 1.0};
				performHeadAttack(head, at, true);
				idle = 0;
			}
		}
		int id = _targets[head];
		if (id > 0) {
			Actor* target = _level.actorById(id);
			if (target && isValidTarget(*target) && distanceToSqr(*target) <= 900.0 && sensing().hasLineOfSight(*target)) {
				AABB box = target->boundingBox();
				performHeadAttack(head, {target->position().x, target->position().y + (box.maxY - box.minY) * 0.5, target->position().z},
								  head == 0 && _random.nextFloat() < 0.001F);
				_nextHeadUpdate[head - 1] = _tickCount + 40 + _random.nextInt(20);
				_idleHeadUpdates[head - 1] = 0;
			} else {
				setAlternativeTarget(head, 0);
			}
		} else {
			std::vector<Actor*> candidates;
			for (Actor* actor : _level.actorsIn(boundingBox().inflate(20.0, 8.0, 20.0), this)) {
				if (isValidTarget(*actor) && sensing().hasLineOfSight(*actor)) candidates.push_back(actor);
			}
			if (!candidates.empty()) setAlternativeTarget(head, candidates[_random.nextInt(static_cast<int>(candidates.size()))]->id());
		}
	}
	Actor* target = getTarget();
	setAlternativeTarget(0, target ? target->id() : 0);
	// Hurt a second ago: the blocks around it break
	if (_destroyBlocksTick > 0 && --_destroyBlocksTick == 0) {
		bool destroyed = false;
		int	 x0 = Mth::floor(_position.x), y0 = Mth::floor(_position.y), z0 = Mth::floor(_position.z);
		for (int dx = -1; dx <= 1; dx++) {
			for (int dz = -1; dz <= 1; dz++) {
				for (int dy = 0; dy <= 3; dy++) {
					BlockPos pos{x0 + dx, y0 + dy, z0 + dz};
					int		 state = _level.getBlockState(pos);
					if (_level.blocks().isAir(state)) continue;
					if (_level.gameData().isInTag("minecraft:block", "minecraft:wither_immune", _level.blocks().blockOf(state))) continue;
					destroyed = _level.destroyBlock(pos, true) || destroyed;
				}
			}
		}
		if (destroyed) _level.levelEvent(nullptr, LEVEL_EVENT_WITHER_BREAK_BLOCK, blockPosition(), 0);
	}
	if (_tickCount % 20 == 0) heal(1.0F);
	_bossBar.setProgress(_health / maxHealth());
	_bossBar.update(_position, 64.0);
}

bool WitherBoss::hurtServer(const Combat::DamageSource& source, float amount) {
	const GameData& data	   = _level.gameData();
	int				damageType = data.getSyncedId("minecraft:damage_type", source.type);
	if (data.isInTag("minecraft:damage_type", "minecraft:wither_immune_to", damageType)) return false;
	if (_invulnerableTicks > 0 && !data.isInTag("minecraft:damage_type", "minecraft:bypasses_invulnerability", damageType)) return false;
	// Powered, its armor stops arrows
	if (isPowered()) {
		Actor* direct = source.directEntity();
		if (direct && direct != source.causing && data.getEntityType(direct->typeId())->is("AbstractArrow")) return false;
	}
	if (source.causing && source.causing != static_cast<Actor*>(this) &&
		data.isInTag("minecraft:entity_type", "minecraft:wither_friends", source.causing->typeId())) {
		return false;
	}
	if (_destroyBlocksTick <= 0) _destroyBlocksTick = 20;
	for (int& idle : _idleHeadUpdates) idle += 3;
	bool hurt = Monster::hurtServer(source, amount);
	if (!isAlive()) _bossBar.removeAll();
	return hurt;
}

void WitherBoss::checkDespawn() {
	if (_level.difficulty() == 0) {
		_bossBar.removeAll();
		discard();
		return;
	}
	_noActionTime = 0;
}

void WitherBoss::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Monster::writeData(buf, mask, onlyNonDefault);
	for (int head = 0; head < 3; head++) {
		if (wantsData(mask, DATA_TARGET_A + head, onlyNonDefault, _targets[head] == 0)) writeIntData(buf, DATA_TARGET_A + head, _targets[head]);
	}
	if (wantsData(mask, DATA_ID_INV, onlyNonDefault, _invulnerableTicks == 0)) writeIntData(buf, DATA_ID_INV, _invulnerableTicks);
}

void WitherBoss::save(Buffer& buf) const {
	Monster::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeInt(_invulnerableTicks);
}

void WitherBoss::load(Buffer& buf) {
	Monster::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown wither data version");
	setInvulnerableTicks(buf.readInt());
}

// ===================== WitherSkull =====================

WitherSkull::WitherSkull(Level& level, LivingEntity* shooter, const Vec3& direction, bool dangerous)
	: HurtingProjectile(level, "minecraft:wither_skull", 0.3125F, shooter, direction), _dangerous(dangerous) {}

void WitherSkull::writeEntityData(Buffer& buf) const {
	if (!_dangerous) return;
	buf.writeUByte(DATA_DANGEROUS);
	buf.writeVarInt(8); // EntityDataSerializers.BOOLEAN
	buf.writeBool(true);
}

void WitherSkull::onHit(const Hit& hit) {
	Actor* by = owner();
	if (hit.actor) {
		bool hurt;
		if (by) {
			hurt = hurtActor(_level, *hit.actor, {"minecraft:wither_skull", by, this, std::nullopt}, 8.0F);
			if (hurt && !hit.actor->isAlive()) {
				if (LivingEntity* living = by->asLiving()) living->heal(5.0F);
			}
		} else {
			hurt = hurtActor(_level, *hit.actor, {"minecraft:magic", nullptr, nullptr, std::nullopt}, 5.0F);
		}
		int seconds = _level.difficulty() == 2 ? 10 : _level.difficulty() == 3 ? 40 : 0;
		if (hurt && seconds > 0 && hit.actor->isAlive()) addEffectTo(*hit.actor, {MobEffects::Wither, 20 * seconds, 1});
	}
	Explosions::explode(_level, this, std::nullopt, nullptr, _position, 1.0F, false, Explosions::Interaction::Mob);
	discard();
}

// ===================== DragonFireball =====================

DragonFireball::DragonFireball(Level& level, LivingEntity* shooter, const Vec3& direction)
	: HurtingProjectile(level, "minecraft:dragon_fireball", 1.0F, shooter, direction) {}

void DragonFireball::onHit(const Hit& hit) {
	Actor* by = owner();
	if (hit.actor && hit.actor == by) return;
	// The dragon's breath: a cloud of instant damage, on the first living thing within 4 blocks if there is one
	Vec3 at = _position;
	for (Actor* actor : _level.actorsIn(boundingBox().inflate(4.0, 2.0, 4.0), this)) {
		if (actor == by || !actor->isAlive() || (!actor->asLiving() && !actor->asPlayer())) continue;
		if (distanceToSqr(actor->position()) < 16.0) {
			at = actor->position();
			break;
		}
	}
	_level.levelEvent(nullptr, 2006, BlockPos{Mth::floor(_position.x), Mth::floor(_position.y), Mth::floor(_position.z)}, 1); // Dragon breath particles
	_level.entities().add(AreaEffectCloud::dragonBreath(_level, at, by));
	discard();
}

// ===================== EnderDragon =====================

EnderDragon::EnderDragon(Level& level, int typeId) : Mob(level, typeId), _bossBar(level, "entity.minecraft.ender_dragon", BossBar::Pink, 2 | 4) {
	// Its parts take the 8 ids after its own (the clients make them so)
	IdManager& ids = level.server().getIdManager();
	ids.release(static_cast<uint32_t>(_id));
	_id = static_cast<int>(ids.allocateRange(1 + PARTS));
	_noPhysics = true;
	setNoGravity(true);
	_xpReward = 500;
}

EnderDragon::~EnderDragon() {
	IdManager& ids = _level.server().getIdManager();
	for (int part = 1; part <= PARTS; part++) ids.release(static_cast<uint32_t>(_id + part));
}

void EnderDragon::setPhase(Phase phase) {
	if (phase == _phase) return;
	_phase		= phase;
	_phaseTicks = 0;
	markData(DATA_PHASE);
}

void EnderDragon::findPodium() {
	// The top of the ground at the center
	int x = Mth::floor(_center.x), z = Mth::floor(_center.z);
	for (int y = std::min(_level.minY() + 320, 255); y > _level.minY(); y--) {
		if (!_level.blocks().isAir(_level.getBlockState({x, y, z}))) {
			_podiumY = y + 1.0;
			return;
		}
	}
	_podiumY = _center.y;
}

std::shared_ptr<SpawnGroupData> EnderDragon::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	_center = _position;
	findPodium();
	_target = _position;
	return Mob::finalizeSpawn(difficulty, reason, group);
}

Vec3 EnderDragon::headPosition() const {
	float yaw = _yRot * (float)M_PI / 180.0F;
	return {_position.x + Mth::sin(yaw) * 6.5, _position.y + 1.0, _position.z - Mth::cos(yaw) * 6.5};
}

void EnderDragon::flyToTarget(float flySpeed, float turnSpeed) {
	// EnderDragon.aiStep's flight toward its phase's target
	double dx = _target.x - _position.x, dy = _target.y - _position.y, dz = _target.z - _position.z;
	double dist2	  = dx * dx + dy * dy + dz * dz;
	double horizontal = std::sqrt(dx * dx + dz * dz);
	if (horizontal > 0.0) dy = Mth::clamp(static_cast<float>(dy / horizontal), -flySpeed, flySpeed);
	_delta.y += dy * 0.01;
	_yRot		= Mth::wrapDegrees(_yRot);
	Vec3  dir	= Vec3{dx, dy, dz}.normalize();
	float yaw	= _yRot * (float)M_PI / 180.0F;
	Vec3  face	= Vec3{Mth::sin(yaw), _delta.y, -Mth::cos(yaw)}.normalize();
	float align = std::max(static_cast<float>((face.x * dir.x + face.y * dir.y + face.z * dir.z + 0.5) / 1.5), 0.0F);
	if (std::abs(dx) > 1.0E-5 || std::abs(dz) > 1.0E-5) {
		float turn = Mth::clamp(Mth::wrapDegrees(180.0F - static_cast<float>(Mth::atan2(dx, dz)) * (180.0F / (float)M_PI) - _yRot), -50.0F, 50.0F);
		_yRotA *= 0.8F;
		_yRotA += turn * turnSpeed;
		_yRot += _yRotA * 0.1F;
	}
	float closeness = static_cast<float>(2.0 / (dist2 + 1.0));
	moveRelative(0.06F * (align * closeness + (1.0F - closeness)), {0.0, 0.0, -1.0});
	move(_delta);
	Vec3  d = _delta.normalize();
	float n = static_cast<float>((d.x * face.x + d.y * face.y + d.z * face.z) + 1.0) / 2.0F;
	n		= 0.8F + 0.15F * n;
	_delta	= _delta.multiply(n, 0.91, n);
}

void EnderDragon::hurtAndKnockBack() {
	// Its wings throw away what they touch, its head bites
	AABB body = boundingBox().inflate(4.0, 2.0, 4.0).move(0.0, -2.0, 0.0);
	for (Actor* actor : _level.actorsIn(body, this)) {
		if (!actor->isAlive() || (!actor->asLiving() && !actor->asPlayer())) continue;
		double dx = actor->position().x - _position.x, dz = actor->position().z - _position.z;
		double d  = std::max(dx * dx + dz * dz, 0.1);
		actor->pushMotion({dx / d * 4.0, 0.2, dz / d * 4.0});
		if (_phase == Phase::ChargingPlayer || _phase == Phase::HoldingPattern || _phase == Phase::StrafePlayer) {
			hurtActor(_level, *actor, {"minecraft:mob_attack", this, nullptr, std::nullopt}, 5.0F);
		}
	}
	Vec3 head = headPosition();
	AABB bite{head.x - 1.5, head.y - 1.5, head.z - 1.5, head.x + 1.5, head.y + 1.5, head.z + 1.5};
	for (Actor* actor : _level.actorsIn(bite, this)) {
		if (!actor->isAlive() || (!actor->asLiving() && !actor->asPlayer())) continue;
		hurtActor(_level, *actor, {"minecraft:mob_attack", this, nullptr, std::nullopt}, 10.0F);
	}
}

void EnderDragon::chooseNextPhase() {
	// DragonHoldingPatternPhase.findNewTarget: now and then it lands; else a player to charge or strafe
	std::vector<Player*> players;
	for (const auto& player : _level.players()) {
		if (!player->isDisconnected() && player->isAlive() && !player->isCreative() && !player->isSpectator() &&
			(player->position() - _center).lengthSqr() < 150.0 * 150.0) {
			players.push_back(player.get());
		}
	}
	if (_random.nextInt(players.empty() ? 3 : 8) == 0) {
		setPhase(Phase::LandingApproach);
		return;
	}
	if (!players.empty() && _random.nextInt(3) != 0) {
		Player* player = players[_random.nextInt(static_cast<int>(players.size()))];
		_attackTarget  = EntityRef(*player);
		setPhase(_random.nextBoolean() ? Phase::ChargingPlayer : Phase::StrafePlayer);
		return;
	}
	setPhase(Phase::HoldingPattern);
}

void EnderDragon::tickPhase() {
	_phaseTicks++;
	Actor* attacked = _attackTarget.isSet() ? _level.actorByRef(_attackTarget) : nullptr;
	switch (_phase) {
		case Phase::HoldingPattern: {
			// Around the center, 40 blocks away, 20 to 40 above the ground
			double angle = _circleStep * M_PI / 6.0;
			_target		 = {_center.x + std::cos(angle) * 40.0, _podiumY + 20.0 + (_circleStep % 3) * 10.0, _center.z + std::sin(angle) * 40.0};
			if ((_target - _position).horizontalDistanceSqr() < 100.0) _circleStep = (_circleStep + 1) % 12;
			flyToTarget(0.6F, 0.7F);
			if (_phaseTicks > 200 && _random.nextInt(100) == 0) chooseNextPhase();
			break;
		}
		case Phase::ChargingPlayer:
			if (!attacked || !attacked->isAlive()) {
				setPhase(Phase::HoldingPattern);
				break;
			}
			if (_phaseTicks == 1) _target = attacked->position();
			flyToTarget(3.0F, 0.7F);
			if ((_target - _position).lengthSqr() < 16.0 || _phaseTicks > 200) setPhase(Phase::HoldingPattern);
			break;
		case Phase::StrafePlayer:
			if (!attacked || !attacked->isAlive()) {
				setPhase(Phase::HoldingPattern);
				break;
			}
			_target = attacked->position() + Vec3{0.0, 10.0, 0.0};
			flyToTarget(0.6F, 0.7F);
			if (--_fireballCooldown <= 0 && distanceToSqr(*attacked) < 64.0 * 64.0 && sensing().hasLineOfSight(*attacked)) {
				// DragonStrafePlayerPhase: a fireball from its head
				Vec3 head = headPosition();
				auto ball = std::make_unique<DragonFireball>(_level, this, attacked->position() - head);
				ball->setPos(head);
				_level.entities().add(std::move(ball));
				_level.levelEvent(nullptr, LEVEL_EVENT_DRAGON_SHOOT, blockPosition(), 0);
				_fireballCooldown = 60;
				setPhase(Phase::LandingApproach == _phase ? _phase : Phase::HoldingPattern);
			}
			if (_phaseTicks > 400) setPhase(Phase::HoldingPattern);
			break;
		case Phase::LandingApproach:
			_target = {_center.x, _podiumY + 10.0, _center.z};
			flyToTarget(0.6F, 0.7F);
			if ((_target - _position).lengthSqr() < 25.0) setPhase(Phase::Landing);
			break;
		case Phase::Landing:
			_target = {_center.x, _podiumY, _center.z};
			flyToTarget(1.5F, 0.7F);
			if ((_target - _position).lengthSqr() < 1.5 || _phaseTicks > 200) {
				_delta = {};
				setPhase(Phase::SittingScanning);
			}
			break;
		case Phase::SittingScanning:
		case Phase::SittingFlaming:
		case Phase::SittingAttacking:
			_delta = {};
			if (_phase == Phase::SittingScanning && _phaseTicks == 20) playSound("minecraft:entity.ender_dragon.growl", 2.5F, 1.0F);
			if (_phaseTicks > 100 + _random.nextInt(100)) setPhase(Phase::Takeoff);
			break;
		case Phase::Takeoff:
			_target = {_center.x, _podiumY + 30.0, _center.z};
			flyToTarget(1.5F, 0.7F);
			if (_position.y > _podiumY + 20.0 || _phaseTicks > 200) setPhase(Phase::HoldingPattern);
			break;
		case Phase::Dying:
		case Phase::Hovering: break;
	}
}

void EnderDragon::aiStep() {
	if (_phase == Phase::Dying) {
		// EnderDragon.tickDeath: it rises for 10 seconds, its experience coming out
		_dyingTicks++;
		_delta = {0.0, 0.1, 0.0};
		move(_delta);
		if (_dyingTicks > 150 && _dyingTicks % 5 == 0) {
			if (auto orb = ExperienceOrb::create(_level, _position, 600 / 12)) _level.entities().add(std::move(orb));
		}
		if (_dyingTicks == 1) _level.levelEvent(nullptr, 1028, blockPosition(), 0); // Its death roar, everywhere
		if (_dyingTicks >= 200) {
			if (auto orb = ExperienceOrb::create(_level, _position, 600 / 2)) _level.entities().add(std::move(orb));
			if (EndDragonFight* fight = _level.endFight()) fight->setDragonKilled();
			_bossBar.removeAll();
			discard();
			return;
		}
		_bossBar.setProgress(0.0F);
		_bossBar.update(_position, 192.0);
		return;
	}
	if (_tickCount == 1 && _center.lengthSqr() == 0.0) {
		_center = _position;
		findPodium();
	}
	checkCrystals();
	tickPhase();
	hurtAndKnockBack();
	_yHeadRot = _yRot;
	_yBodyRot = _yRot;
	_bossBar.setProgress(_health / maxHealth());
	_bossBar.update(_position, 192.0);
}

void EnderDragon::checkCrystals() {
	Actor* crystal = _healingCrystal >= 0 ? _level.actorById(_healingCrystal) : nullptr;
	if (crystal && (!crystal->isAlive() || !dynamic_cast<EndCrystal*>(crystal))) crystal = nullptr;
	if (!crystal) _healingCrystal = -1;
	if (crystal && _tickCount % 10 == 0 && _health < maxHealth()) setHealth(_health + 1.0F);
	if (_random.nextInt(10) == 0) {
		double best = 0.0;
		Actor* nearest = nullptr;
		for (Actor* actor : _level.actorsIn(boundingBox().inflate(32.0), this)) {
			if (!dynamic_cast<EndCrystal*>(actor) || !actor->isAlive()) continue;
			double d = actor->distanceToSqr(*this);
			if (!nearest || d < best) nearest = actor, best = d;
		}
		_healingCrystal = nearest ? nearest->id() : -1;
	}
}

bool EnderDragon::hurtServer(const Combat::DamageSource& source, float amount) {
	if (_phase == Phase::Dying) return false;
	const GameData& data	   = _level.gameData();
	int				damageType = data.getSyncedId("minecraft:damage_type", source.type);
	if (data.isInTag("minecraft:damage_type", "minecraft:is_explosion", damageType) && source.causing == static_cast<Actor*>(this)) return false;
	// Sitting, it shrugs off most of it
	if (_phase == Phase::SittingScanning || _phase == Phase::SittingFlaming || _phase == Phase::SittingAttacking) amount *= 0.25F;
	if (amount >= _health) {
		// Not dead yet: it dies slowly
		setHealth(1.0F);
		setPhase(Phase::Dying);
		playSound("minecraft:entity.ender_dragon.death", 5.0F, 1.0F);
		return true;
	}
	bool hurt = Mob::hurtServer(source, amount);
	if (hurt && (_phase == Phase::SittingScanning || _phase == Phase::SittingFlaming) && _random.nextInt(3) == 0) setPhase(Phase::Takeoff);
	return hurt;
}

void EnderDragon::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Mob::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_PHASE, onlyNonDefault, _phase == Phase::Hovering)) writeIntData(buf, DATA_PHASE, static_cast<int>(_phase));
}

void EnderDragon::save(Buffer& buf) const {
	Mob::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeInt(static_cast<int>(_phase));
	buf.writeDouble(_center.x);
	buf.writeDouble(_center.y);
	buf.writeDouble(_center.z);
	buf.writeDouble(_podiumY);
	buf.writeInt(_dyingTicks);
}

void EnderDragon::load(Buffer& buf) {
	Mob::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown ender dragon data version");
	int phase = buf.readInt();
	_center.x = buf.readDouble();
	_center.y = buf.readDouble();
	_center.z = buf.readDouble();
	_podiumY  = buf.readDouble();
	_dyingTicks = buf.readInt();
	setPhase(static_cast<Phase>(std::max(0, std::min(10, phase))));
}

// ===================== WitherSkullBehavior =====================

void WitherSkullBehavior::onPlace(Level& level, const BlockPos& pos, int, int, bool) const {
	// WitherSkullBlock.checkSpawn: the pattern "^^^" / "###" / "~#~" (skulls, soul sand or soil, air), along x or z
	const GameData& data	 = level.gameData();
	auto			isSkull = [&](const BlockPos& p) {
		   std::string name = data.getStaticName("minecraft:block", level.blocks().blockOf(level.getBlockState(p)));
		   return name == "minecraft:wither_skeleton_skull" || name == "minecraft:wither_skeleton_wall_skull";
	};
	auto isBase = [&](const BlockPos& p) { return data.isInTag("minecraft:block", "minecraft:wither_summon_base_blocks", level.blocks().blockOf(level.getBlockState(p))); };
	auto isAir	= [&](const BlockPos& p) { return level.blocks().isAir(level.getBlockState(p)); };
	if (pos.y < level.minY() + 2) return;
	for (int axis = 0; axis < 2; axis++) {
		int ax = axis == 0 ? 1 : 0, az = axis == 0 ? 0 : 1;
		for (int offset = -2; offset <= 0; offset++) {
			BlockPos left = pos.offset(ax * offset, 0, az * offset);
			BlockPos cols[3] = {left, left.offset(ax, 0, az), left.offset(2 * ax, 0, 2 * az)};
			bool	 match	 = true;
			for (const BlockPos& c : cols) match = match && isSkull(c) && isBase(c.below());
			if (!match || !isBase(cols[1].offset(0, -2, 0)) || !isAir(cols[0].offset(0, -2, 0)) || !isAir(cols[2].offset(0, -2, 0))) continue;
			// Summoned: the pattern goes, a charging wither stands in its middle
			int air = data.getDefaultBlockState("minecraft:air");
			for (const BlockPos& c : cols) {
				level.setBlock(c, air, Level::UPDATE_CLIENTS);
				level.setBlock(c.below(), air, Level::UPDATE_CLIENTS);
			}
			level.setBlock(cols[1].offset(0, -2, 0), air, Level::UPDATE_CLIENTS);
			BlockPos bottom = cols[1].offset(0, -2, 0);
			Mob*	 mob	= level.mobs().spawn(level, data.getStaticId("minecraft:entity_type", "minecraft:wither"), bottom, MobRegistry::SpawnReason::Triggered);
			if (auto* wither = dynamic_cast<WitherBoss*>(mob)) {
				wither->setPos({bottom.x + 0.5, bottom.y + 0.55, bottom.z + 0.5});
				wither->setYRotPublic(axis == 0 ? 0.0F : 90.0F);
				wither->setYBodyRot(axis == 0 ? 0.0F : 90.0F);
				wither->makeInvulnerable();
			}
			return;
		}
	}
}
