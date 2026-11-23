#include "world/entity/mobs/Warden.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/MobRegistry.hpp"
#include "world/entity/ai/Goals.hpp"

#include <cmath>
#include <stdexcept>

namespace {
	constexpr int SAVE_VERSION				 = 1;
	constexpr int VIBRATION_COOLDOWN_TICKS = 40, DARKNESS_INTERVAL = 120, DARKNESS_DURATION = 260, SONIC_BOOM_DURATION = 60;
	constexpr int SONIC_BOOM_SOUND_DELAY	 = 34;

	// While it emerges, digs or roars it does nothing else
	class WardenBusyGoal : public Goal {
	  public:
		explicit WardenBusyGoal(Warden& warden) : _warden(warden) { setFlags({Flag::Move, Flag::Look, Flag::Jump}); }
		bool canUse() override { return _warden.isDiggingOrEmerging() || _warden.roarTicks > 0; }
		bool isInterruptable() override { return false; }
		void start() override { _warden.navigation().stop(); }

	  private:
		Warden& _warden;
	};

	// Warden's SonicBoom: from up to 15 blocks (20 up or down), out of its swipe's reach, a charged boom through
	// everything, every few seconds
	class WardenSonicBoomGoal : public Goal {
	  public:
		explicit WardenSonicBoomGoal(Warden& warden) : _warden(warden) { setFlags({Flag::Move, Flag::Look}); }
		bool canUse() override {
			Actor* target = _warden.getTarget();
			if (!target || !target->isAlive() || _warden.sonicBoomCooldown > 0) return false;
			Vec3 to = target->position() - _warden.position();
			if (to.horizontalDistanceSqr() > 15.0 * 15.0 || std::abs(to.y) > 20.0) return false;
			return !_warden.isWithinMeleeAttackRange(*target) && (_warden.navigation().isStuck() || to.horizontalDistanceSqr() > 9.0 ||
																   !_warden.sensing().hasLineOfSight(*target) || _warden.random().nextInt(4) == 0);
		}
		bool canContinueToUse() override { return _ticks < SONIC_BOOM_DURATION && _warden.getTarget(); }
		void start() override {
			_ticks = 0;
			_warden.navigation().stop();
			_warden.playSound("minecraft:entity.warden.sonic_charge", 3.0F, 1.0F);
		}
		void stop() override { _warden.sonicBoomCooldown = Warden::SONIC_BOOM_COOLDOWN; }
		bool requiresUpdateEveryTick() override { return true; }
		void tick() override {
			Actor* target = _warden.getTarget();
			if (!target) return;
			_warden.lookControl().setLookAt(*target, 30.0F, 30.0F);
			if (++_ticks == SONIC_BOOM_SOUND_DELAY) _warden.sonicBoom(*target);
		}

	  private:
		Warden& _warden;
		int		_ticks = 0;
	};

	// The investigation of a vibration it isn't angry enough to fight for: it walks to where it came from
	class WardenInvestigateGoal : public Goal {
	  public:
		explicit WardenInvestigateGoal(Warden& warden) : _warden(warden) { setFlags({Flag::Move}); }
		bool canUse() override { return _hasSpot && !_warden.getTarget(); }
		bool canContinueToUse() override { return _hasSpot && !_warden.getTarget() && _warden.navigation().isInProgress(); }
		void start() override { _warden.navigation().moveTo(_spot.x, _spot.y, _spot.z, 0.7); }
		void stop() override { _hasSpot = false; }
		void investigate(const Vec3& spot) {
			_spot	 = spot;
			_hasSpot = true;
		}

	  private:
		Warden& _warden;
		Vec3	_spot;
		bool	_hasSpot = false;
	};
} // namespace

Warden::Warden(Level& level, int typeId) : Monster(level, typeId) {
	_xpReward = 5;
	setPathfindingMalus(PathType::DamageOther, 8.0F);
	setPathfindingMalus(PathType::PowderSnow, 8.0F);
	setPathfindingMalus(PathType::Lava, 8.0F);
	setPathfindingMalus(PathType::DamageFire, 0.0F);
	setPathfindingMalus(PathType::DangerFire, 0.0F);
}

void Warden::registerGoals() {
	_goalSelector.addGoal(0, std::make_unique<WardenBusyGoal>(*this));
	_goalSelector.addGoal(1, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(2, std::make_unique<WardenSonicBoomGoal>(*this));
	_goalSelector.addGoal(3, std::make_unique<MeleeAttackGoal>(*this, 1.2, true));
	_goalSelector.addGoal(4, std::make_unique<WardenInvestigateGoal>(*this));
	_goalSelector.addGoal(6, std::make_unique<RandomStrollGoal>(*this, 0.5));
}

bool Warden::canTargetEntity(Actor& actor) {
	if (&actor == this || !actor.isAlive() || actor.isSpectator()) return false;
	if (!actor.asLiving() && !actor.asPlayer()) return false;
	if (Player* player = actor.asPlayer(); player && player->isCreative()) return false;
	const GameData::EntityTypeInfo* type = _level.gameData().getEntityType(actor.typeId());
	if (type && (type->is("Warden") || type->is("ArmorStand"))) return false;
	return (actor.position() - _position).lengthSqr() <= 48.0 * 48.0;
}

int Warden::angerAt(Actor& actor) const {
	auto it = _anger.find(actor.uuid());
	return it == _anger.end() ? 0 : it->second;
}

void Warden::increaseAngerAt(Actor& actor, int amount, bool sound) {
	if (isNoAi() || !canTargetEntity(actor)) return;
	int& anger	= _anger[actor.uuid()];
	bool before = anger >= ANGERY;
	anger		= std::min(MAX_ANGER, anger + amount);
	if (!before && anger >= ANGERY && sound) playSound("minecraft:entity.warden.angry", 10.0F, 1.0F);
	syncClientAnger();
}

void Warden::clearAngerAt(Actor& actor) {
	_anger.erase(actor.uuid());
	syncClientAnger();
}

int Warden::activeAnger() const {
	int max = 0;
	for (const auto& [uuid, anger] : _anger) max = std::max(max, anger);
	return max;
}

Actor* Warden::angriestTarget() {
	// AngerManagement.getActiveEntity: the angriest one it may still target (players first when as angry)
	Actor* best		 = nullptr;
	int	   bestAnger = 0;
	for (Actor* actor : _level.actorsIn(boundingBox().inflate(48.0), this)) {
		if (!canTargetEntity(*actor)) continue;
		int anger = angerAt(*actor);
		if (anger > bestAnger || (anger == bestAnger && anger > 0 && actor->asPlayer() && best && !best->asPlayer())) {
			best	  = actor;
			bestAnger = anger;
		}
	}
	return bestAnger >= ANGERY ? best : nullptr;
}

void Warden::syncClientAnger() {
	int anger = activeAnger();
	if (anger == _clientAnger) return;
	_clientAnger = anger;
	markData(DATA_CLIENT_ANGER_LEVEL);
}

void Warden::setTarget(Actor* target) {
	bool had = getTarget() != nullptr;
	Monster::setTarget(target);
	if (target && !had) {
		// Roar: it found what it hunts
		startPose(POSE_ROARING);
		roarTicks = ROAR_TICKS;
		playSound("minecraft:entity.warden.roar", 3.0F, 1.0F);
	}
}

void Warden::startPose(int pose) {
	setPose(pose);
	_poseTicks = 0;
}

void Warden::hearVibration(Actor& source) {
	_digCooldown = DIG_COOLDOWN;
	if (isDiggingOrEmerging()) return;
	increaseAngerAt(source, VIBRATION_ANGER, true);
	playSound(activeAnger() >= ANGERY ? "minecraft:entity.warden.listening_angry" : "minecraft:entity.warden.listening", 5.0F, 1.0F);
	if (angerAt(source) < ANGERY && !getTarget()) {
		for (const auto& wrapped : _goalSelector.goals()) {
			if (auto* investigate = dynamic_cast<WardenInvestigateGoal*>(&wrapped->goal())) investigate->investigate(source.position());
		}
	}
}

void Warden::listen() {
	// Its vibration listener: whatever moved within 16 blocks without sneaking, one vibration every 2 seconds
	std::unordered_map<int, Vec3> positions;
	Actor*						  heard = nullptr;
	double						  closest = 0.0;
	for (Actor* actor : _level.actorsIn(boundingBox().inflate(16.0), this)) {
		if (!actor->asLiving() && !actor->asPlayer()) continue;
		positions[actor->id()] = actor->position();
		auto last			   = _lastPositions.find(actor->id());
		if (last == _lastPositions.end()) continue;
		bool moved = (actor->position() - last->second).lengthSqr() > 1.0E-4;
		if (Player* player = actor->asPlayer(); player && player->isShiftKeyDown()) moved = false; // Sneaking dampens its steps
		if (!moved || !canTargetEntity(*actor)) continue;
		double d = actor->distanceToSqr(*this);
		if (d > 16.0 * 16.0) continue;
		if (!heard || d < closest) heard = actor, closest = d;
	}
	_lastPositions = std::move(positions);
	if (heard && _tickCount % VIBRATION_COOLDOWN_TICKS == 0) hearVibration(*heard);
}

void Warden::sonicBoom(Actor& target) {
	// Warden's SonicBoom: through blocks and shields, knocked back along the beam
	playSound("minecraft:entity.warden.sonic_boom", 3.0F, 1.0F);
	broadcastEntityEvent(EVENT_SONIC_BOOM);
	Combat::DamageSource source{"minecraft:sonic_boom", this, nullptr, std::nullopt};
	bool hurt = target.asPlayer() ? Combat::damage(_level.server(), *target.asPlayer(), 10.0F, source) : target.hurtServer(source, 10.0F);
	if (!hurt) return;
	double resistance = 0.0;
	if (LivingEntity* living = target.asLiving()) resistance = living->getAttributeValue(living->attributeIds().knockbackResistance);
	Vec3 direction = (target.position() - _position).normalize();
	target.pushMotion({direction.x * 2.5 * (1.0 - resistance), direction.y * 0.5 * (1.0 - resistance), direction.z * 2.5 * (1.0 - resistance)});
}

bool Warden::doHurtTarget(Actor& target) {
	broadcastEntityEvent(EVENT_ATTACK);
	playSound("minecraft:entity.warden.attack_impact", 10.0F, (_random.nextFloat() - _random.nextFloat()) * 0.2F + 1.0F);
	sonicBoomCooldown = std::max(sonicBoomCooldown, SONIC_BOOM_COOLDOWN);
	return Monster::doHurtTarget(target);
}

bool Warden::hurtServer(const Combat::DamageSource& source, float amount) {
	if (isDiggingOrEmerging()) return false;
	bool hurt = Monster::hurtServer(source, amount);
	if (hurt && isAlive() && source.causing) {
		Actor& attacker = *source.causing;
		_digCooldown	= DIG_COOLDOWN;
		increaseAngerAt(attacker, HIT_ANGER, false);
		// Hit by something it can fight while it hunts something else it is less angry at: it switches
		Actor* target = getTarget();
		if (canTargetEntity(attacker) && angerAt(attacker) >= ANGERY && (!target || angerAt(*target) < angerAt(attacker))) Monster::setTarget(&attacker);
	}
	return hurt;
}

void Warden::customServerAiStep() {
	_poseTicks++;
	if (sonicBoomCooldown > 0) sonicBoomCooldown--;
	if (roarTicks > 0 && --roarTicks == 0 && _pose == POSE_ROARING) startPose(POSE_STANDING);
	if (isEmerging()) {
		if (_poseTicks >= EMERGE_TICKS) startPose(POSE_STANDING);
		Monster::customServerAiStep();
		return;
	}
	if (isDigging()) {
		if (_poseTicks >= DIG_TICKS) discard();
		return;
	}
	listen();
	// Anger fades by one a second, and goes for those it can't target anymore
	if (_tickCount % 20 == 0) {
		for (auto it = _anger.begin(); it != _anger.end();) {
			if (--it->second <= 0) {
				it = _anger.erase(it);
			} else {
				++it;
			}
		}
		syncClientAnger();
	}
	// Its target: the one it is angriest at (80 and more)
	Actor* target = getTarget();
	if (target && (!canTargetEntity(*target) || angerAt(*target) < ANGERY)) {
		Monster::setTarget(nullptr);
		target = nullptr;
	}
	if (!target) {
		if (Actor* angriest = angriestTarget()) setTarget(angriest);
	}
	// The darkness around it
	if (_tickCount % DARKNESS_INTERVAL == 0) {
		for (Actor* actor : _level.actorsIn(boundingBox().inflate(20.0), this)) {
			Player* player = actor->asPlayer();
			if (!player || player->isCreative() || player->isSpectator()) continue;
			const MobEffects::Instance* darkness = player->effects().get(MobEffects::Darkness);
			if (!darkness || darkness->duration < 200) player->addEffect({MobEffects::Darkness, DARKNESS_DURATION, 0});
		}
	}
	// Calm for a minute: it digs back down
	if (activeAnger() == 0 && !getTarget()) {
		if (--_digCooldown <= 0) {
			startPose(POSE_DIGGING);
			navigation().stop();
			playSound("minecraft:entity.warden.dig", 5.0F, 1.0F);
		}
	} else {
		_digCooldown = DIG_COOLDOWN;
	}
	Monster::customServerAiStep();
}

std::shared_ptr<SpawnGroupData> Warden::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	// Called by a shrieker (EntitySpawnReason.TRIGGERED), it emerges from the ground
	if (reason == static_cast<int>(MobRegistry::SpawnReason::Triggered)) {
		startPose(POSE_EMERGING);
		playSound("minecraft:entity.warden.emerge", 5.0F, 1.0F);
	}
	return Monster::finalizeSpawn(difficulty, reason, group);
}

std::string Warden::ambientSoundName() {
	if (isDiggingOrEmerging() || _pose == POSE_ROARING) return "";
	int anger = activeAnger();
	return anger >= ANGERY ? "minecraft:entity.warden.angry" : anger >= ANGERY / 2 ? "minecraft:entity.warden.agitated" : "minecraft:entity.warden.ambient";
}

void Warden::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Monster::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_CLIENT_ANGER_LEVEL, onlyNonDefault, _clientAnger == 0)) writeIntData(buf, DATA_CLIENT_ANGER_LEVEL, _clientAnger);
}

void Warden::save(Buffer& buf) const {
	Monster::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeInt(static_cast<int>(_anger.size()));
	for (const auto& [uuid, anger] : _anger) {
		buf.writeLong(static_cast<int64_t>(uuid.getMostSigBits()));
		buf.writeLong(static_cast<int64_t>(uuid.getLeastSigBits()));
		buf.writeInt(anger);
	}
	buf.writeInt(_digCooldown);
}

void Warden::load(Buffer& buf) {
	Monster::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown warden data version");
	_anger.clear();
	int count = buf.readInt();
	for (int i = 0; i < count; i++) {
		uint64_t most  = static_cast<uint64_t>(buf.readLong());
		uint64_t least = static_cast<uint64_t>(buf.readLong());
		_anger[UUID(most, least)] = buf.readInt();
	}
	_digCooldown = buf.readInt();
	syncClientAnger();
}
