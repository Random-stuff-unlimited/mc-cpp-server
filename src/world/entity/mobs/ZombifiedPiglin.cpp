#include "world/entity/mobs/ZombifiedPiglin.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/ai/Goals.hpp"

#include <algorithm>

void ZombifiedPiglin::addBehaviourGoals() {
	_goalSelector.addGoal(2, std::make_unique<ZombieAttackGoal>(*this, 1.0, false));
	_goalSelector.addGoal(7, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 1.0));
	// Hurting one makes the whole group around join in (ZombifiedPiglin, neutral)
	auto hurtBy = std::make_unique<HurtByTargetGoal>(*this);
	hurtBy->setAlertOthers({"ZombifiedPiglin"});
	_targetSelector.addGoal(1, std::move(hurtBy));
	// Neutral: players are only targeted while it is angry
	_targetSelector.addGoal(2, std::make_unique<NearestAttackableTargetGoal>(*this, "Player", 10, true, false,
																		   [this](Actor&, Level&) { return _angerTime > 0; }));
}

bool ZombifiedPiglin::hurtServer(const Combat::DamageSource& source, float amount) {
	bool result = Zombie::hurtServer(source, amount);
	if (result && !isRemoved()) {
		// ZombifiedPiglin.setTarget when hurt by a player: angry for 20 seconds
		if (Player* player = source.attackerPlayer()) {
			_angerTime = std::max(_angerTime, 400);
			if (!getTarget()) setTarget(player);
		}
	}
	return result;
}

void ZombifiedPiglin::aiStep() {
	if (_angerTime > 0) _angerTime--;
	Zombie::aiStep();
}

void ZombifiedPiglin::save(Buffer& buf) const {
	Zombie::save(buf);
	buf.writeVarInt(_angerTime);
}

void ZombifiedPiglin::load(Buffer& buf) {
	Zombie::load(buf);
	_angerTime = buf.readVarInt();
}