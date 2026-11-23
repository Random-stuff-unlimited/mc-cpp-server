#ifndef WARDEN_HPP
#define WARDEN_HPP

#include "lib/UUID.hpp"
#include "world/entity/mobs/Monster.hpp"

#include <unordered_map>

struct UUIDHash {
	size_t operator()(const UUID& uuid) const { return std::hash<uint64_t>()(uuid.getMostSigBits() * 31 + uuid.getLeastSigBits()); }
};

// Warden: blind, it hears the vibrations within 16 blocks (whatever moves without sneaking, and what hits it) and
// gets angrier at their source (35 a vibration, 100 a hit, -1 a second). At 80 it hunts it: a 30 damage swipe, or a
// sonic boom (10 damage through armor and shields) from up to 15 blocks. It darkens the players around, emerges from
// the ground when summoned by a shrieker, and digs back down after a minute without anger. Its smelling (sniffing)
// is replaced by hearing, and vibrations of blocks (placed, broken) aren't heard
class Warden : public Monster {
  public:
	static constexpr int DATA_CLIENT_ANGER_LEVEL = 16;
	static constexpr int POSE_ROARING = 11, POSE_SNIFFING = 12, POSE_EMERGING = 13, POSE_DIGGING = 14;
	static constexpr int ANGERY = 80, MAX_ANGER = 150, VIBRATION_ANGER = 35, HIT_ANGER = 100;
	static constexpr int EMERGE_TICKS = 134, DIG_TICKS = 100, ROAR_TICKS = 84, DIG_COOLDOWN = 1200, SONIC_BOOM_COOLDOWN = 40;
	static constexpr int EVENT_ATTACK = 4, EVENT_SONIC_BOOM = 62;

	Warden(Level& level, int typeId);

	// AngerManagement: its anger at each one (by UUID), the angriest it can attack first
	int	   angerAt(Actor& actor) const;
	void   increaseAngerAt(Actor& actor, int amount, bool playSound);
	void   clearAngerAt(Actor& actor);
	int	   activeAnger() const;
	Actor* angriestTarget();
	bool   isEmerging() const { return _pose == POSE_EMERGING; }
	bool   isDigging() const { return _pose == POSE_DIGGING; }
	bool   isDiggingOrEmerging() const { return isEmerging() || isDigging(); }
	// Warden.canTargetEntity: a living thing (not a warden, not invulnerable, not creative nor spectator) here
	bool   canTargetEntity(Actor& actor);
	// The vibrations it heard this tick (VibrationSystem: its listener), and what they make it do
	void   hearVibration(Actor& source);
	void   sonicBoom(Actor& target);

	void							registerGoals() override;
	void							setTarget(Actor* target) override;
	bool							hurtServer(const Combat::DamageSource& source, float amount) override;
	bool							doHurtTarget(Actor& target) override;
	bool							isPushable() const override { return !isDiggingOrEmerging(); }
	bool							removeWhenFarAway(double) const override { return false; }
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	std::string						ambientSoundName() override;
	void							save(Buffer& buf) const override;
	void							load(Buffer& buf) override;
	using Monster::playSound;

	int sonicBoomCooldown = 0, roarTicks = 0;

  protected:
	void customServerAiStep() override;
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;
	int	 ambientSoundInterval() const override { return 160; }

  private:
	std::unordered_map<UUID, int, UUIDHash> _anger;
	std::unordered_map<int, Vec3>			_lastPositions; // Where what it listens to was (entity id), to hear it move
	int										_poseTicks = 0, _digCooldown = DIG_COOLDOWN, _clientAnger = 0, _heartbeat = 0;

	void startPose(int pose);
	void listen();
	void syncClientAnger();
};

#endif
