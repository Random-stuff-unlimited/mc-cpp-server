#include "world/entity/mobs/IronGolem.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/PoiManager.hpp"
#include "world/entity/ai/Goals.hpp"
#include "world/entity/ai/RandomPos.hpp"
#include "world/item/Enchantments.hpp"

#include <cmath>

namespace {
	constexpr int SAVE_VERSION = 1;

	int sectionsToVillage(Level& level, int x, int y, int z) { return level.poi().sectionsToVillage(x, y, z); }
	// ServerLevel.isVillage
	bool isVillage(Level& level, const BlockPos& pos) { return sectionsToVillage(level, pos.x >> 4, pos.y >> 4, pos.z >> 4) <= 1; }

	bool isEnemyButCreeper(Level& level, Actor& target) {
		const GameData::EntityTypeInfo* type = level.gameData().getEntityType(target.typeId());
		return type && type->is("Enemy") && !type->is("Creeper");
	}

	// MoveBackToVillageGoal: out of its village, toward the nearest section that is closer to it
	class MoveBackToVillageGoal : public RandomStrollGoal {
	  public:
		MoveBackToVillageGoal(Mob& mob, double speed, bool checkNoActionTime) : RandomStrollGoal(mob, speed, 10, checkNoActionTime) {}
		bool canUse() override { return isVillage(_mob.level(), _mob.blockPosition()) ? false : RandomStrollGoal::canUse(); }

	  protected:
		std::optional<Vec3> getPosition() override {
			// BehaviorUtils.findSectionClosestToVillage(level, section, 2)
			Level&	 level = _mob.level();
			BlockPos pos   = _mob.blockPosition();
			int		 sx = pos.x >> 4, sy = pos.y >> 4, sz = pos.z >> 4;
			int		 best = sectionsToVillage(level, sx, sy, sz);
			int		 bx = sx, by = sy, bz = sz;
			bool	 found = false;
			for (int z = sz - 2; z <= sz + 2; z++) {
				for (int y = sy - 2; y <= sy + 2; y++) {
					for (int x = sx - 2; x <= sx + 2; x++) {
						int distance = sectionsToVillage(level, x, y, z);
						if (distance < best) {
							best  = distance;
							bx = x, by = y, bz = z;
							found = true;
						}
					}
				}
			}
			if (!found) return std::nullopt;
			return RandomPos::defaultPosTowards(_mob, 10, 7, {bx * 16 + 8 + 0.5, static_cast<double>(by * 16 + 8), bz * 16 + 8 + 0.5}, M_PI / 2);
		}
	};

	// GolemRandomStrollInVillageGoal: toward a point of interest of the village sections around (or a villager that
	// wants a golem: villagers have no class yet), anywhere 30 % of the time
	class GolemRandomStrollInVillageGoal : public RandomStrollGoal {
	  public:
		GolemRandomStrollInVillageGoal(Mob& mob, double speed) : RandomStrollGoal(mob, speed, 240, false) {}

	  protected:
		std::optional<Vec3> getPosition() override {
			JavaRandom& random = _mob.level().random();
			random.nextFloat(); // Which of the villager or the point of interest comes first
			if (random.nextFloat() < 0.3F) return RandomPos::landPos(_mob, 10, 7);
			std::optional<Vec3> pos = towardsPoi();
			return pos ? pos : RandomPos::landPos(_mob, 10, 7);
		}

	  private:
		std::optional<Vec3> towardsPoi() {
			Level&	 level = _mob.level();
			BlockPos pos   = _mob.blockPosition();
			int		 sx = pos.x >> 4, sy = pos.y >> 4, sz = pos.z >> 4;
			std::vector<BlockPos> sections;
			for (int z = sz - 2; z <= sz + 2; z++) {
				for (int y = sy - 2; y <= sy + 2; y++) {
					for (int x = sx - 2; x <= sx + 2; x++) {
						if (sectionsToVillage(level, x, y, z) == 0) sections.push_back({x, y, z});
					}
				}
			}
			if (sections.empty()) return std::nullopt;
			const BlockPos& section = sections[level.random().nextInt(static_cast<int>(sections.size()))];
			BlockPos		center{section.x * 16 + 8, section.y * 16 + 8, section.z * 16 + 8};
			auto			records = level.poi().getAllInRange(center, 8, PoiManager::Occupancy::IsOccupied);
			if (records.empty()) return std::nullopt;
			const BlockPos& poi = records[level.random().nextInt(static_cast<int>(records.size()))]->pos;
			return RandomPos::landPosTowards(_mob, 10, 7, {poi.x + 0.5, static_cast<double>(poi.y), poi.z + 0.5});
		}
	};

	// OfferFlowerGoal: now and then by day, holds a poppy out to a villager nearby for 20 seconds
	class OfferFlowerGoal : public Goal {
	  public:
		explicit OfferFlowerGoal(IronGolem& golem) : _golem(golem) { setFlags({Flag::Move, Flag::Look}); }
		bool canUse() override {
			if (!_golem.level().isBrightOutside()) return false;
			if (_golem.random().nextInt(8000) != 0) return false;
			Level&				level = _golem.level();
			std::vector<Actor*> candidates;
			level.entities().forEachIn(_golem.boundingBox().inflate(6.0, 2.0, 6.0), [&](Entity& entity) {
				if (!entity.isRemoved() && entity.asLiving() && &entity != &_golem &&
					level.gameData().isInTag("minecraft:entity_type", "minecraft:candidate_for_iron_golem_gift", entity.typeId())) {
					candidates.push_back(&entity);
				}
			});
			TargetingConditions conditions = TargetingConditions::forNonCombat();
			conditions.withRange(6.0);
			Actor* nearest = nearestActor(level, candidates, conditions, &_golem, _golem.position().x, _golem.position().y, _golem.position().z);
			if (!nearest) return false;
			_entity = EntityRef(*nearest);
			return true;
		}
		bool canContinueToUse() override { return _tick > 0; }
		void start() override {
			_tick = adjustedTickDelay(400);
			_golem.offerFlower(true);
		}
		void stop() override {
			_golem.offerFlower(false); // The copper golem's poppy isn't given
			_entity.clear();
		}
		void tick() override {
			if (Actor* entity = _entity.isSet() ? _golem.level().actorByRef(_entity) : nullptr) _golem.lookControl().setLookAt(*entity, 30.0F, 30.0F);
			_tick--;
		}

	  private:
		IronGolem& _golem;
		EntityRef  _entity;
		int		   _tick = 0;
	};
} // namespace

void IronGolem::registerGoals() {
	_goalSelector.addGoal(1, std::make_unique<MeleeAttackGoal>(*this, 1.0, true));
	_goalSelector.addGoal(2, std::make_unique<MoveTowardsTargetGoal>(*this, 0.9, 32.0F));
	_goalSelector.addGoal(2, std::make_unique<MoveBackToVillageGoal>(*this, 0.6, false));
	_goalSelector.addGoal(4, std::make_unique<GolemRandomStrollInVillageGoal>(*this, 0.6));
	_goalSelector.addGoal(5, std::make_unique<OfferFlowerGoal>(*this));
	_goalSelector.addGoal(7, std::make_unique<LookAtPlayerGoal>(*this, "Player", 6.0F));
	_goalSelector.addGoal(8, std::make_unique<RandomLookAroundGoal>(*this));
	// DefendVillageTargetGoal (the players the villagers hate) needs the villagers' reputations: not there yet
	_targetSelector.addGoal(2, std::make_unique<HurtByTargetGoal>(*this));
	_targetSelector.addGoal(3, std::make_unique<NearestAttackableTargetGoal>(*this, "Player", 10, true, false,
																		   [this](Actor& target, Level&) { return _anger.isAngryAt(target); }));
	_targetSelector.addGoal(3, std::make_unique<NearestAttackableTargetGoal>(*this, "Mob", 5, false, false,
																		   [](Actor& target, Level& level) { return isEnemyButCreeper(level, target); }));
	// ResetUniversalAngerTargetGoal: the universalAnger game rule is off
}

void IronGolem::setPlayerCreated(bool created) {
	uint8_t flags = static_cast<uint8_t>(created ? _flags | 1 : _flags & ~1);
	if (flags == _flags) return;
	_flags = flags;
	markData(DATA_FLAGS);
}

void IronGolem::offerFlower(bool offer) {
	_offerFlowerTick = offer ? 400 : 0;
	broadcastEntityEvent(offer ? EVENT_OFFER_FLOWER : EVENT_STOP_OFFER_FLOWER);
}

void IronGolem::doPush(Entity& other) {
	if (isEnemyButCreeper(_level, other) && _random.nextInt(20) == 0) setTarget(&other);
}

void IronGolem::aiStep() {
	Mob::aiStep();
	if (_attackAnimationTick > 0) _attackAnimationTick--;
	if (_offerFlowerTick > 0) _offerFlowerTick--;
	if (!isRemoved()) _anger.update(true);
}

bool IronGolem::canAttackType(int typeId) const {
	const std::string& name = _level.gameData().getStaticName("minecraft:entity_type", typeId);
	if (isPlayerCreated() && name == "minecraft:player") return false;
	return name == "minecraft:creeper" ? false : Mob::canAttackType(typeId);
}

bool IronGolem::doHurtTarget(Actor& target) {
	_attackAnimationTick = 10;
	broadcastEntityEvent(EVENT_ATTACK);
	float damage = static_cast<float>(getAttributeValue(_ids.attackDamage));
	float dealt	 = static_cast<int>(damage) > 0 ? damage / 2.0F + _random.nextInt(static_cast<int>(damage)) : damage;
	Combat::DamageSource source{"minecraft:mob_attack", this, nullptr, std::nullopt};
	bool hurt = target.asPlayer() ? Combat::damage(_level.server(), *target.asPlayer(), dealt, source) : target.hurtServer(source, dealt);
	if (hurt) {
		// Thrown up, less with knockback resistance
		double resistance = 0.0;
		if (LivingEntity* living = target.asLiving()) resistance = living->getAttributeValue(living->attributeIds().knockbackResistance);
		target.pushMotion({0.0, 0.4F * std::max(0.0, 1.0 - resistance), 0.0});
		setLastHurtMob(&target);
	}
	playSound("minecraft:entity.iron_golem.attack", 1.0F, 1.0F);
	return hurt;
}

int IronGolem::crackiness() const {
	float fraction = _health / maxHealth();
	return fraction < 0.25F ? 3 : fraction < 0.5F ? 2 : fraction < 0.75F ? 1 : 0;
}

bool IronGolem::hurtServer(const Combat::DamageSource& source, float amount) {
	int	 before = crackiness();
	bool hurt	= Mob::hurtServer(source, amount);
	if (hurt && crackiness() != before) playSound("minecraft:entity.iron_golem.damage", 1.0F, 1.0F);
	return hurt;
}

bool IronGolem::mobInteract(Player& player, int hand) {
	const ItemStack& stack = player.getStackInHand(hand);
	if (stack.isEmpty() || stack.item != _level.gameData().getStaticId("minecraft:item", "minecraft:iron_ingot")) return false;
	float before = _health;
	heal(25.0F);
	if (_health == before) return false;
	playSound("minecraft:entity.iron_golem.repair", 1.0F, 1.0F + (_random.nextFloat() - _random.nextFloat()) * 0.2F);
	if (!player.isCreative()) player.inventory().getMutable(player.handSlot(hand)).shrink(1);
	return true;
}

void IronGolem::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Mob::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_FLAGS, onlyNonDefault, _flags == 0)) writeByteData(buf, DATA_FLAGS, _flags);
}

void IronGolem::save(Buffer& buf) const {
	Mob::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeBool(isPlayerCreated());
	_anger.save(buf);
}

void IronGolem::load(Buffer& buf) {
	Mob::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown iron golem data version");
	_flags = buf.readBool() ? 1 : 0;
	_anger.load(buf);
	_dirtyData = 0;
}
