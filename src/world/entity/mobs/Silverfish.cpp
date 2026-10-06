#include "world/entity/mobs/Silverfish.hpp"

#include "PacketIds.hpp"
#include "network/buffer.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/ai/Goals.hpp"

namespace {
	constexpr int EVENT_SPAWN_ANIM = 20;

	bool isInfested(Level& level, int block) {
		const std::string& name = level.gameData().getStaticName("minecraft:block", block);
		return name.rfind("minecraft:infested_", 0) == 0;
	}

	// Silverfish.SilverfishWakeUpFriendsGoal: a second after it was hurt, the infested blocks around break open
	class WakeUpFriendsGoal : public Goal {
	  public:
		explicit WakeUpFriendsGoal(Silverfish& silverfish) : _silverfish(silverfish) {}
		void notifyHurt() {
			if (_lookForFriends == 0) _lookForFriends = adjustedTickDelay(20);
		}
		bool canUse() override { return _lookForFriends > 0; }
		void tick() override {
			if (--_lookForFriends > 0) return;
			Level&		level  = _silverfish.level();
			JavaRandom& random = _silverfish.random();
			BlockPos	center = _silverfish.blockPosition();
			// From the center outward: 0, 1, -1, 2, -2...
			for (int y = 0; y <= 5 && y >= -5; y = (y <= 0 ? 1 : 0) - y) {
				for (int x = 0; x <= 10 && x >= -10; x = (x <= 0 ? 1 : 0) - x) {
					for (int z = 0; z <= 10 && z >= -10; z = (z <= 0 ? 1 : 0) - z) {
						BlockPos pos = center.offset(x, y, z);
						if (!isInfested(level, level.blocks().blockOf(level.getBlockState(pos)))) continue;
						level.destroyBlock(pos, true); // mobGriefing: always on here
						if (random.nextBoolean()) return;
					}
				}
			}
		}

	  private:
		Silverfish& _silverfish;
		int			_lookForFriends = 0;
	};

	// Silverfish.SilverfishMergeWithStoneGoal: idle, it enters a stone block next to it; else it strolls
	class MergeWithStoneGoal : public RandomStrollGoal {
	  public:
		explicit MergeWithStoneGoal(Silverfish& silverfish) : RandomStrollGoal(silverfish, 1.0, 10) { setFlags({Flag::Move}); }
		bool canUse() override {
			if (_mob.getTarget() || !_mob.navigation().isDone()) return false;
			JavaRandom& random = _mob.random();
			if (random.nextInt(reducedTickDelay(10)) == 0) {
				_direction = Directions::ALL[random.nextInt(6)]; // Direction.getRandom
				if (Silverfish::infestedStateOf(_mob.level(), _mob.level().getBlockState(target())) >= 0) {
					_doMerge = true;
					return true;
				}
			}
			_doMerge = false;
			return RandomStrollGoal::canUse();
		}
		bool canContinueToUse() override { return _doMerge ? false : RandomStrollGoal::canContinueToUse(); }
		void start() override {
			if (!_doMerge) {
				RandomStrollGoal::start();
				return;
			}
			Level&	 level	  = _mob.level();
			BlockPos pos	  = target();
			int		 infested = Silverfish::infestedStateOf(level, level.getBlockState(pos));
			if (infested < 0) return;
			level.setBlock(pos, infested, Level::UPDATE_ALL);
			// Mob.spawnAnim: the poof, for its viewers
			Buffer event;
			event.writeInt(_mob.id());
			event.writeByte(EVENT_SPAWN_ANIM);
			level.entities().broadcast(_mob, PacketId::Play::Clientbound::ENTITY_EVENT, event);
			_mob.discard();
		}

	  private:
		Direction _direction = Direction::Down;
		bool	  _doMerge	 = false;
		BlockPos  target() const {
			 const Vec3& p = _mob.position();
			 return BlockPos{Mth::floor(p.x), Mth::floor(p.y + 0.5), Mth::floor(p.z)}.relative(_direction);
		}
	};
} // namespace

int Silverfish::infestedStateOf(Level& level, int hostState) {
	// InfestedBlock.HOST_TO_INFESTED_STATES: the host's infested twin, with its properties (the deepslate's axis)
	const GameData&	   data = level.gameData();
	const std::string& name = data.getStaticName("minecraft:block", level.blocks().blockOf(hostState));
	static const char* hosts[] = {"stone", "cobblestone", "stone_bricks", "mossy_stone_bricks", "cracked_stone_bricks", "chiseled_stone_bricks", "deepslate"};
	for (const char* host : hosts) {
		if (name != std::string("minecraft:") + host) continue;
		std::string state = data.getBlockStateName(hostState);
		return data.getBlockStateFromName("minecraft:infested_" + state.substr(std::string("minecraft:").size()));
	}
	return -1;
}

void Silverfish::registerGoals() {
	auto friends = std::make_shared<WakeUpFriendsGoal>(*this);
	_friendsGoal = friends;
	_goalSelector.addGoal(1, std::make_unique<FloatGoal>(*this));
	// ClimbOnTopOfPowderSnowGoal: powder snow isn't walked on yet
	_goalSelector.addSharedGoal(3, _friendsGoal);
	_goalSelector.addGoal(4, std::make_unique<MeleeAttackGoal>(*this, 1.0, false));
	_goalSelector.addGoal(5, std::make_unique<MergeWithStoneGoal>(*this));
	auto hurtBy = std::make_unique<HurtByTargetGoal>(*this);
	hurtBy->setAlertOthers();
	_targetSelector.addGoal(1, std::move(hurtBy));
	_targetSelector.addGoal(2, std::make_unique<NearestAttackableTargetGoal>(*this, "Player", true));
}

bool Silverfish::hurtServer(const Combat::DamageSource& source, float amount) {
	const GameData& data = _level.gameData();
	bool always = data.isInTag("minecraft:damage_type", "minecraft:always_triggers_silverfish", data.getSyncedId("minecraft:damage_type", source.type));
	if ((source.causing || always) && _friendsGoal) static_cast<WakeUpFriendsGoal&>(*_friendsGoal).notifyHurt();
	return Monster::hurtServer(source, amount);
}

void Silverfish::tick() {
	_yBodyRot = _yRot;
	Monster::tick();
}

float Silverfish::getWalkTargetValue(const BlockPos& pos) {
	return infestedStateOf(_level, _level.getBlockState(pos.below())) >= 0 ? 10.0F : Monster::getWalkTargetValue(pos);
}
