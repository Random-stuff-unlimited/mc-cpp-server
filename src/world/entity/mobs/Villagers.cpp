#include "world/entity/mobs/IronGolem.hpp"
#include "world/entity/mobs/Villagers.hpp"

#include "PacketIds.hpp"
#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/PoiManager.hpp"
#include "world/entity/ExperienceOrb.hpp"
#include "world/entity/ItemEntity.hpp"
#include "world/entity/MobRegistry.hpp"
#include "world/entity/ai/Goals.hpp"
#include "world/entity/mobs/Zombie.hpp"
#include "world/inventory/MerchantMenu.hpp"
#include "world/item/Components.hpp"

#include <cmath>
#include <stdexcept>

namespace {
	constexpr int SAVE_VERSION				 = 1;
	constexpr int SERIALIZER_VILLAGER_DATA	 = 18, SERIALIZER_OPTIONAL_BLOCK_POS = 11;
	constexpr int EVENT_VILLAGER_HEARTS		 = 12;
	constexpr int POI_SEARCH_RANGE			 = 48;

	// The professions in PoiManager::Type's order (armorer to weaponsmith)
	const char* const PROFESSIONS[] = {"minecraft:armorer",	  "minecraft:butcher", "minecraft:cartographer", "minecraft:cleric",  "minecraft:farmer",
									   "minecraft:fisherman", "minecraft:fletcher", "minecraft:leatherworker", "minecraft:librarian", "minecraft:mason",
									   "minecraft:shepherd",  "minecraft:toolsmith", "minecraft:weaponsmith"};
	constexpr int PROFESSION_COUNT = 13;

	bool isItem(Level& level, const ItemStack& stack, const char* name) {
		return !stack.isEmpty() && stack.item == level.gameData().getStaticId("minecraft:item", name);
	}
	int foodValue(Level& level, const ItemStack& stack) {
		if (isItem(level, stack, "minecraft:bread")) return 4;
		if (isItem(level, stack, "minecraft:carrot") || isItem(level, stack, "minecraft:potato") || isItem(level, stack, "minecraft:beetroot")) return 1;
		return 0;
	}

	// The villager's day: to its job site, the bell, or its bed (and into it), by its schedule
	class VillagerScheduleGoal : public Goal {
	  public:
		explicit VillagerScheduleGoal(Villager& villager) : _villager(villager) { setFlags({Flag::Move, Flag::Look, Flag::Jump}); }
		bool canUse() override { return place().has_value() && !_villager.isPanicking(); }
		bool canContinueToUse() override { return canUse(); }
		void start() override { _repath = 0; }
		void stop() override {
			if (_villager.isSleeping()) _villager.stopSleeping();
			_villager.navigation().stop();
		}
		void tick() override {
			std::optional<BlockPos> target = place();
			if (!target) return;
			if (_villager.isSleeping()) return;
			Vec3   center{target->x + 0.5, static_cast<double>(target->y), target->z + 0.5};
			double close = _villager.activity() == Villager::Activity::Meet ? 4.0 : 1.5;
			double d	 = (center - _villager.position()).horizontalDistanceSqr();
			if (_villager.activity() == Villager::Activity::Rest && d < 2.0 && std::abs(center.y - _villager.position().y) < 1.5) {
				_villager.navigation().stop();
				_villager.startSleeping(*target);
				return;
			}
			if (d > close * close) {
				if (--_repath <= 0 || _villager.navigation().isDone()) {
					_repath = 20;
					_villager.navigation().moveTo(center.x, center.y, center.z, 0.7);
				}
			} else {
				// There: it potters about (and works now and then)
				_villager.lookControl().setLookAt(center.x, center.y + 0.5, center.z);
				if (_villager.activity() == Villager::Activity::Work && _villager.random().nextInt(200) == 0) {
					std::string sound = _villager.professionName();
					sound			  = "minecraft:entity.villager.work_" + sound.substr(sound.find(':') + 1);
					_villager.playSound(sound, 1.0F, 1.0F);
				}
				if (_villager.navigation().isDone() && _villager.random().nextInt(120) == 0) {
					JavaRandom& random = _villager.random();
					_villager.navigation().moveTo(center.x + random.nextInt(5) - 2, center.y, center.z + random.nextInt(5) - 2, 0.5);
				}
			}
		}

	  private:
		Villager& _villager;
		int		  _repath = 0;

		std::optional<BlockPos> place() {
			switch (_villager.activity()) {
				case Villager::Activity::Work: return _villager.hasJob() ? _villager.jobSite() : std::nullopt;
				case Villager::Activity::Meet: return _villager.meetingPoint();
				case Villager::Activity::Rest: return _villager.home();
				default: return std::nullopt;
			}
		}
	};

	// LookAndFollowTradingPlayerSink / TradeWithPlayerGoal: it stays and looks at its customer
	class TradeWithPlayerGoal : public Goal {
	  public:
		explicit TradeWithPlayerGoal(AbstractVillager& merchant) : _merchant(merchant) { setFlags({Flag::Move, Flag::Look, Flag::Jump}); }
		bool canUse() override { return _merchant.isAlive() && _merchant.tradingPlayer() != nullptr; }
		void start() override { _merchant.navigation().stop(); }
		void tick() override {
			if (Player* player = _merchant.tradingPlayer()) _merchant.lookControl().setLookAt(*player);
		}

	  private:
		AbstractVillager& _merchant;
	};

	// A sleeping villager does nothing else
	class VillagerSleepingGoal : public Goal {
	  public:
		explicit VillagerSleepingGoal(Villager& villager) : _villager(villager) { setFlags({Flag::Move, Flag::Look, Flag::Jump}); }
		bool canUse() override { return _villager.isSleeping(); }

	  private:
		Villager& _villager;
	};

	// The wandering trader's wander target, and away from it once there
	class WanderToPositionGoal : public Goal {
	  public:
		WanderToPositionGoal(WanderingTrader& trader, std::optional<BlockPos>& target) : _trader(trader), _target(target) { setFlags({Flag::Move}); }
		bool canUse() override { return _target && !_trader.isPanicking() && (Vec3{_target->x + 0.5, 0.0, _target->z + 0.5} - Vec3{_trader.position().x, 0.0, _trader.position().z}).horizontalDistanceSqr() > 4.0; }
		void tick() override {
			if (!_target) return;
			if (_trader.navigation().isDone()) {
				// Toward it, ten blocks at a time
				Vec3 to = Vec3{_target->x + 0.5, static_cast<double>(_target->y), _target->z + 0.5} - _trader.position();
				Vec3 step = to.lengthSqr() > 100.0 ? to.normalize().scale(10.0) : to;
				Vec3 at	  = _trader.position() + step;
				_trader.navigation().moveTo(at.x, at.y, at.z, 0.35);
			}
		}
		void stop() override {
			if (_target && (Vec3{_target->x + 0.5, 0.0, _target->z + 0.5} - Vec3{_trader.position().x, 0.0, _trader.position().z}).horizontalDistanceSqr() <= 4.0) _target.reset();
		}

	  private:
		WanderingTrader&		 _trader;
		std::optional<BlockPos>& _target;
	};
} // namespace

// ===================== AbstractVillager =====================

AbstractVillager::AbstractVillager(Level& level, int typeId) : AgeableMob(level, typeId) {
	setPathfindingMalus(PathType::DangerFire, 16.0F);
	setPathfindingMalus(PathType::DamageFire, -1.0F);
}

bool AbstractVillager::isHostile(Level& level, Actor& actor) {
	static const char* const HOSTILES[] = {"minecraft:drowned", "minecraft:evoker",	 "minecraft:husk",		 "minecraft:illusioner",
										   "minecraft:pillager", "minecraft:ravager", "minecraft:vex",		 "minecraft:vindicator",
										   "minecraft:zoglin",	 "minecraft:zombie",  "minecraft:zombie_villager"};
	const std::string& name = level.gameData().getStaticName("minecraft:entity_type", actor.typeId());
	for (const char* hostile : HOSTILES) {
		if (name == hostile) return actor.isAlive();
	}
	return false;
}

void AbstractVillager::setUnhappy() {
	_unhappy = 40;
	markData(DATA_UNHAPPY_COUNTER);
	playSound(_type.is("WanderingTrader") ? "minecraft:entity.wandering_trader.no" : "minecraft:entity.villager.no", 1.0F, 1.0F);
}

bool AbstractVillager::mobInteract(Player& player, int hand) {
	// Villager.mobInteract: spawn eggs and name tags as usual; else its trades, or a shake of the head
	const ItemStack& held = player.getStackInHand(hand);
	if (!held.isEmpty() && (_level.gameData().getSpawnEggType(held.item) >= 0 || held.item == _level.gameData().getStaticId("minecraft:item", "minecraft:name_tag"))) {
		return AgeableMob::mobInteract(player, hand);
	}
	if (!isAlive() || isBaby() || isTrading() || player.isShiftKeyDown()) {
		if (hand == 0 && isAlive()) setUnhappy();
		return hand == 0;
	}
	if (hand == 1) return false; // The main hand opens it
	if (offers().empty()) {
		setUnhappy();
		return true;
	}
	navigation().stop();
	Menus::openMerchant(player, _level, *this);
	return true;
}

std::vector<MerchantOffer>& AbstractVillager::offers() {
	if (_offers.empty()) updateTrades();
	return _offers;
}

Player* AbstractVillager::tradingPlayer() {
	Actor*	actor  = _tradingPlayer.isSet() ? _level.actorByRef(_tradingPlayer) : nullptr;
	Player* player = actor ? actor->asPlayer() : nullptr;
	return player && player->isAlive() ? player : nullptr;
}

void AbstractVillager::setTradingPlayer(Player* player) {
	if (player) {
		_tradingPlayer = EntityRef(*player);
	} else {
		_tradingPlayer.clear();
	}
}

void AbstractVillager::notifyTrade(MerchantOffer& offer) {
	// AbstractVillager.notifyTrade: one more use, the trade sound, and some experience for the player (rewardTradeXp)
	offer.uses++;
	_ambientSoundTime = -ambientSoundInterval();
	playSound(tradeSound(), 1.0F, 1.0F);
	if (offer.rewardExp) {
		if (auto orb = ExperienceOrb::create(_level, {_position.x, _position.y + 0.5, _position.z}, 3 + _random.nextInt(4))) _level.entities().add(std::move(orb));
	}
}

void AbstractVillager::save(Buffer& buf) const {
	AgeableMob::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeVarInt(static_cast<int>(_offers.size()));
	for (const MerchantOffer& offer : _offers) offer.save(buf);
}

void AbstractVillager::load(Buffer& buf) {
	AgeableMob::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown merchant data version");
	_offers.clear();
	int count = buf.readVarInt();
	for (int i = 0; i < count; i++) _offers.push_back(MerchantOffer::load(buf));
}

void AbstractVillager::customServerAiStep() {
	if (_unhappy > 0 && --_unhappy == 0) markData(DATA_UNHAPPY_COUNTER);
	AgeableMob::customServerAiStep();
}

void AbstractVillager::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	AgeableMob::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_UNHAPPY_COUNTER, onlyNonDefault, _unhappy == 0)) writeIntData(buf, DATA_UNHAPPY_COUNTER, _unhappy);
}

// ===================== Villager =====================

Villager::Villager(Level& level, int typeId) : AbstractVillager(level, typeId) {
	_villagerType = level.gameData().getStaticId("minecraft:villager_type", "minecraft:plains");
	_profession	  = level.gameData().getStaticId("minecraft:villager_profession", "minecraft:none");
	setCanPickUpLoot(true);
}

void Villager::registerGoals() {
	_goalSelector.addGoal(0, std::make_unique<VillagerSleepingGoal>(*this));
	_goalSelector.addGoal(1, std::make_unique<TradeWithPlayerGoal>(*this));
	_goalSelector.addGoal(1, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(2, std::make_unique<AvoidEntityGoal>(*this, "LivingEntity", 8.0F, 0.6, 0.6,
															   [this](Actor& actor) { return AbstractVillager::isHostile(_level, actor); }));
	_goalSelector.addGoal(2, std::make_unique<PanicGoal>(*this, 0.6));
	_goalSelector.addGoal(4, std::make_unique<VillagerScheduleGoal>(*this));
	_goalSelector.addGoal(6, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 0.5));
	_goalSelector.addGoal(7, std::make_unique<LookAtPlayerGoal>(*this, "Player", 8.0F));
	_goalSelector.addGoal(8, std::make_unique<LookAtPlayerGoal>(*this, "Villager", 8.0F));
	_goalSelector.addGoal(9, std::make_unique<RandomLookAroundGoal>(*this));
}

std::string Villager::professionName() const { return _level.gameData().getStaticName("minecraft:villager_profession", _profession); }

bool Villager::hasJob() const {
	std::string name = professionName();
	return name != "minecraft:none" && name != "minecraft:nitwit";
}

void Villager::setVillagerData(int type, int profession, int level) {
	_villagerType  = type;
	_profession	   = profession;
	_villagerLevel = level;
	markData(DATA_VILLAGER_DATA);
}

Villager::Activity Villager::activity() const {
	int time = static_cast<int>(((_level.getDayTime() % 24000) + 24000) % 24000);
	if (isBaby()) {
		// VillagerSchedule.SIMPLE (babies): play twice a day
		if (time >= 12000) return Activity::Rest;
		if ((time >= 3000 && time < 6000) || time >= 10000) return Activity::Play;
		return Activity::Idle;
	}
	if (time >= 12000) return Activity::Rest;
	if (time >= 11000) return Activity::Idle;
	if (time >= 9000) return Activity::Meet;
	if (time >= 2000) return Activity::Work;
	return Activity::Idle;
}

void Villager::setBedOccupied(const BlockPos& bed, bool occupied) {
	int		  state = _level.getBlockState(bed);
	std::string current = _level.gameData().getProperty(state, "occupied");
	if (current.empty() || (current == "true") == occupied) return;
	int next = _level.gameData().withProperty(state, "occupied", occupied ? "true" : "false");
	if (next >= 0) _level.setBlock(bed, next, Level::UPDATE_ALL);
}

void Villager::startSleeping(const BlockPos& bed) {
	// LivingEntity.startSleeping: on the bed, lying down
	setPos({bed.x + 0.5, bed.y + 0.6875, bed.z + 0.5});
	_oldPosition = _position;
	_sleeping	 = bed;
	_lastSlept	 = _level.getGameTime(); // LAST_SLEPT
	setBedOccupied(bed, true);
	setPose(POSE_SLEEPING);
	markData(DATA_SLEEPING_POS);
}

void Villager::stopSleeping() {
	if (!_sleeping) return;
	BlockPos bed = *_sleeping;
	_sleeping.reset();
	setBedOccupied(bed, false);
	// Back up beside the bed
	setPos({bed.x + 0.5, bed.y + 0.5625, bed.z + 0.5});
	setPose(POSE_STANDING);
	markData(DATA_SLEEPING_POS);
}

int Villager::foodPoints() const {
	int points = 0;
	for (const ItemStack& stack : _food) points += foodValue(_level, stack) * stack.count;
	return points;
}

void Villager::eatFoodPoints(int points) {
	for (ItemStack& stack : _food) {
		int value = foodValue(_level, stack);
		while (value > 0 && points > 0 && stack.count > 0) {
			stack.shrink(1);
			points -= value;
		}
	}
	std::erase_if(_food, [](const ItemStack& stack) { return stack.count <= 0; });
}

void Villager::addFood(int item, int count) {
	for (ItemStack& stack : _food) {
		if (stack.item == item && stack.count < 64) {
			int moved = std::min(64 - stack.count, count);
			stack.grow(moved);
			count -= moved;
		}
	}
	if (count > 0 && _food.size() < 8) _food.push_back(ItemStack(item, count));
}

bool Villager::wantsToPickUp(const ItemStack& stack) {
	// Villager.WANTED_ITEMS: food, and for farmers what they plant
	if (foodValue(_level, stack) > 0) return _food.size() < 8 || foodPoints() < 64;
	if (professionName() == "minecraft:farmer") {
		return isItem(_level, stack, "minecraft:wheat") || isItem(_level, stack, "minecraft:wheat_seeds") || isItem(_level, stack, "minecraft:beetroot_seeds");
	}
	return false;
}

void Villager::pickUpItem(ItemEntity& item) {
	ItemStack stack = item.item();
	addFood(stack.item, stack.count);
	Buffer take;
	take.writeVarInt(item.id());
	take.writeVarInt(_id);
	take.writeVarInt(stack.count);
	_level.entities().broadcast(item, PacketId::Play::Clientbound::TAKE_ITEM_ENTITY, take);
	item.discard();
}

void Villager::releasePoi(std::optional<BlockPos>& pos) {
	if (!pos) return;
	_level.poi().release(*pos);
	pos.reset();
}

void Villager::acquirePoi() {
	PoiManager& poi	 = _level.poi();
	BlockPos	here = blockPosition();
	// Its job site gone: an apprentice loses its profession
	if (_jobSite) {
		const PoiManager::Record* record = poi.get(*_jobSite);
		if (!record || static_cast<int>(record->type) >= PROFESSION_COUNT || PROFESSIONS[static_cast<int>(record->type)] != professionName()) {
			_jobSite.reset();
			if (_villagerLevel <= 1 && _villagerXp == 0) {
				setVillagerData(_villagerType, _level.gameData().getStaticId("minecraft:villager_profession", "minecraft:none"), 1);
				_offers.clear();
			}
		}
	}
	if (_home && !poi.get(*_home)) _home.reset();
	if (_meetingPoint && !poi.get(*_meetingPoint)) _meetingPoint.reset();
	// AcquirePoi: the nearest free one of each kind within 48 blocks
	auto nearest = [&](PoiManager::Type type, PoiManager::Occupancy occupancy) -> std::optional<BlockPos> {
		std::optional<BlockPos> best;
		int64_t					bestDist = 0;
		for (const PoiManager::Record* record : poi.getInRange(type, here, POI_SEARCH_RANGE, occupancy)) {
			int64_t dx = record->pos.x - here.x, dy = record->pos.y - here.y, dz = record->pos.z - here.z;
			int64_t d  = dx * dx + dy * dy + dz * dz;
			if (!best || d < bestDist) best = record->pos, bestDist = d;
		}
		return best;
	};
	if (!isBaby() && !_jobSite && !isNitwit()) {
		std::optional<BlockPos> best;
		int						bestType = -1;
		int64_t					bestDist = 0;
		for (int type = 0; type < PROFESSION_COUNT; type++) {
			// An employed villager only takes a site of its own profession back
			if (hasJob() && PROFESSIONS[type] != professionName()) continue;
			std::optional<BlockPos> site = nearest(static_cast<PoiManager::Type>(type), PoiManager::Occupancy::HasSpace);
			if (!site) continue;
			int64_t dx = site->x - here.x, dy = site->y - here.y, dz = site->z - here.z, d = dx * dx + dy * dy + dz * dz;
			if (!best || d < bestDist) best = site, bestType = type, bestDist = d;
		}
		if (best && poi.take(*best)) {
			_jobSite = best;
			if (!hasJob()) {
				setVillagerData(_villagerType, _level.gameData().getStaticId("minecraft:villager_profession", PROFESSIONS[bestType]), _villagerLevel);
				playSound("minecraft:entity.villager.celebrate", 1.0F, 1.0F);
			}
		}
	}
	if (!_home) {
		if (std::optional<BlockPos> bed = nearest(PoiManager::Type::Home, PoiManager::Occupancy::HasSpace); bed && poi.take(*bed)) _home = bed;
	}
	if (!_meetingPoint) {
		// A ticket of the bell (32 of them): the village is occupied
		if (std::optional<BlockPos> bell = nearest(PoiManager::Type::Meeting, PoiManager::Occupancy::HasSpace); bell && poi.take(*bell)) _meetingPoint = bell;
	}
}

void Villager::updateTrades() {
	// Villager.updateTrades: two offers of each level it reached
	if (!hasJob()) return;
	for (int level = static_cast<int>(_offers.size()) / 2 + 1; level <= _villagerLevel; level++) {
		VillagerTrades::addOffers(_level, _random, professionName(), level, _offers);
	}
}

void Villager::notifyTrade(MerchantOffer& offer) {
	AbstractVillager::notifyTrade(offer);
	// Villager.rewardTradeXp: its own experience; enough of it, and it will reach the next level (5 more for the player)
	_villagerXp += offer.xp;
	if (_villagerLevel < 5 && _villagerXp >= VillagerTrades::xpForLevel(_villagerLevel + 1)) {
		_updateMerchantTimer			 = 40;
		_increaseProfessionLevelOnUpdate = true;
		if (auto orb = ExperienceOrb::create(_level, {_position.x, _position.y + 0.5, _position.z}, 5)) _level.entities().add(std::move(orb));
	}
	markData(DATA_VILLAGER_DATA);
}

void Villager::increaseMerchantCareer() {
	setVillagerData(_villagerType, _profession, _villagerLevel + 1);
	updateTrades();
}

void Villager::restock() {
	for (MerchantOffer& offer : _offers) {
		offer.updateDemand();
		offer.resetUses();
	}
	_lastRestockGameTime = _level.getGameTime();
	_restocksToday++;
}

void Villager::tryBreed() {
	// VillagerMakeLove: two willing adults (enough food) and a free bed: a baby
	if (isBaby() || _age != 0 || isSleeping() || foodPoints() < BREEDING_FOOD) return;
	Villager* partner = nullptr;
	for (Actor* actor : _level.actorsIn(boundingBox().inflate(8.0), this)) {
		auto* other = dynamic_cast<Villager*>(actor);
		if (other && other->isAlive() && !other->isBaby() && other->getAge() == 0 && !other->isSleeping() && other->foodPoints() >= BREEDING_FOOD) {
			partner = other;
			break;
		}
	}
	if (!partner) return;
	if (_level.poi().getInRange(PoiManager::Type::Home, blockPosition(), POI_SEARCH_RANGE, PoiManager::Occupancy::HasSpace).empty()) return;
	std::unique_ptr<Mob> baby = _level.mobs().create(_level, _typeId);
	auto*				 child = dynamic_cast<Villager*>(baby.get());
	if (!child) return;
	eatFoodPoints(BREEDING_FOOD);
	partner->eatFoodPoints(BREEDING_FOOD);
	setAge(6000);
	partner->setAge(6000);
	child->setBaby(true);
	child->setVillagerData(_random.nextBoolean() ? _villagerType : partner->_villagerType,
						   _level.gameData().getStaticId("minecraft:villager_profession", "minecraft:none"), 1);
	child->snapTo({(_position.x + partner->position().x) / 2.0, _position.y, (_position.z + partner->position().z) / 2.0}, 0.0F, 0.0F);
	broadcastEntityEvent(EVENT_VILLAGER_HEARTS);
	partner->broadcastEntityEvent(EVENT_VILLAGER_HEARTS);
	_level.addFreshEntity(std::move(baby));
}

void Villager::customServerAiStep() {
	int64_t now = _level.getGameTime();
	// VillagerPanicTrigger: scared, it calls for a golem (3 villagers are enough) every 5 seconds
	if (isPanicking() && now % 100 == 0) spawnGolemIfNeeded(now, 3);
	// Villager.gossip, when it meets another villager (at most once a minute each): 5 villagers make a golem
	if (_lastGossip < 0 || now >= _lastGossip + 1200) {
		for (Actor* actor : _level.actorsIn(boundingBox().inflate(2.0, 1.0, 2.0), this)) {
			auto* other = dynamic_cast<Villager*>(actor);
			if (!other || !other->isAlive() || (other->_lastGossip >= 0 && now < other->_lastGossip + 1200)) continue;
			_lastGossip = other->_lastGossip = now;
			spawnGolemIfNeeded(now, 5);
			break;
		}
	}
	// Leveling up, once its customer left: new offers and a moment of regeneration
	if (!isTrading() && _updateMerchantTimer > 0 && --_updateMerchantTimer <= 0) {
		if (_increaseProfessionLevelOnUpdate) {
			increaseMerchantCareer();
			_increaseProfessionLevelOnUpdate = false;
		}
		addEffect({MobEffects::Regeneration, 200, 0});
	}
	// Villager.shouldRestock: at its job site during work, at most twice a day, 2 minutes apart
	int64_t day = _level.getDayTime() / 24000;
	if (day != _lastRestockCheckDay) {
		_lastRestockCheckDay = day;
		_restocksToday		 = 0;
	}
	if (activity() == Activity::Work && _jobSite && _restocksToday < 2 && _level.getGameTime() - _lastRestockGameTime > 2400) {
		Vec3 site{_jobSite->x + 0.5, static_cast<double>(_jobSite->y), _jobSite->z + 0.5};
		bool used = std::any_of(_offers.begin(), _offers.end(), [](const MerchantOffer& offer) { return offer.uses > 0; });
		if (used && (site - _position).lengthSqr() < 2.5 * 2.5) restock();
	}
	if (_sleeping) {
		navigation().stop();
		if (activity() != Activity::Rest || !_home || *_home != *_sleeping) stopSleeping();
	}
	if ((_tickCount + _id) % 40 == 0) acquirePoi();
	if (_breedCooldown > 0) {
		_breedCooldown--;
	} else if ((_tickCount + _id) % 20 == 0) {
		tryBreed();
		_breedCooldown = 100;
	}
	AbstractVillager::customServerAiStep();
}

bool Villager::hurtServer(const Combat::DamageSource& source, float amount) {
	bool hurt = AbstractVillager::hurtServer(source, amount);
	if (hurt && _sleeping) stopSleeping();
	return hurt;
}

void Villager::dropAllDeathLoot(const Combat::DamageSource& source) {
	AbstractVillager::dropAllDeathLoot(source);
	if (_sleeping) stopSleeping();
	releasePoi(_jobSite);
	releasePoi(_home);
	releasePoi(_meetingPoint);
	for (const ItemStack& stack : _food) spawnAtLocation(stack);
	_food.clear();
}

std::shared_ptr<SpawnGroupData> Villager::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	// Villager.finalizeSpawn: the type of its biome
	int type = _level.gameData().getStaticId("minecraft:villager_type", villagerTypeOfBiome(_level.gameData(), _level.getBiomeId(blockPosition())));
	setVillagerData(type >= 0 ? type : _villagerType, _profession, _villagerLevel);
	return AbstractVillager::finalizeSpawn(difficulty, reason, group);
}

std::string Villager::ambientSoundName() {
	if (_sleeping) return "";
	return "minecraft:entity.villager.ambient";
}

void Villager::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	AbstractVillager::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_SLEEPING_POS, onlyNonDefault, !_sleeping)) {
		buf.writeUByte(static_cast<uint8_t>(DATA_SLEEPING_POS));
		buf.writeVarInt(SERIALIZER_OPTIONAL_BLOCK_POS);
		buf.writeBool(_sleeping.has_value());
		if (_sleeping) buf.writePosition(_sleeping->x, _sleeping->y, _sleeping->z);
	}
	// Always sent: the client draws a profession of its own otherwise
	if (wantsData(mask, DATA_VILLAGER_DATA, onlyNonDefault, false)) {
		buf.writeUByte(static_cast<uint8_t>(DATA_VILLAGER_DATA));
		buf.writeVarInt(SERIALIZER_VILLAGER_DATA);
		buf.writeVarInt(_villagerType);
		buf.writeVarInt(_profession);
		buf.writeVarInt(_villagerLevel);
	}
}

void Villager::save(Buffer& buf) const {
	AbstractVillager::save(buf);
	buf.writeUByte(SAVE_VERSION);
	const GameData& data = _level.gameData();
	buf.writeString(data.getStaticName("minecraft:villager_type", _villagerType));
	buf.writeString(data.getStaticName("minecraft:villager_profession", _profession));
	buf.writeVarInt(_villagerLevel);
	buf.writeVarInt(_villagerXp);
	for (const std::optional<BlockPos>* pos : {&_jobSite, &_home}) {
		buf.writeBool(pos->has_value());
		if (*pos) buf.writeLong((*pos)->asLong());
	}
	buf.writeVarInt(static_cast<int>(_food.size()));
	for (const ItemStack& stack : _food) {
		buf.writeVarInt(stack.item);
		buf.writeVarInt(stack.count);
	}
}

void Villager::load(Buffer& buf) {
	AbstractVillager::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown villager data version");
	const GameData& data	   = _level.gameData();
	int				type	   = data.getStaticId("minecraft:villager_type", buf.readString());
	int				profession = data.getStaticId("minecraft:villager_profession", buf.readString());
	int				level	   = buf.readVarInt();
	_villagerXp				   = buf.readVarInt();
	setVillagerData(type >= 0 ? type : _villagerType, profession >= 0 ? profession : _profession, level);
	for (std::optional<BlockPos>* pos : {&_jobSite, &_home}) {
		if (buf.readBool()) {
			*pos = BlockPos::fromLong(buf.readLong());
		} else {
			pos->reset();
		}
	}
	_food.clear();
	int count = buf.readVarInt();
	for (int i = 0; i < count; i++) {
		int item = buf.readVarInt(), n = buf.readVarInt();
		if (n > 0) _food.push_back(ItemStack(item, n));
	}
}

// ===================== WanderingTrader =====================

WanderingTrader::WanderingTrader(Level& level, int typeId) : AbstractVillager(level, typeId) {}

void WanderingTrader::registerGoals() {
	_goalSelector.addGoal(0, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(1, std::make_unique<TradeWithPlayerGoal>(*this));
	_goalSelector.addGoal(1, std::make_unique<AvoidEntityGoal>(*this, "LivingEntity", 8.0F, 0.5, 0.5,
															   [this](Actor& actor) { return AbstractVillager::isHostile(_level, actor); }));
	_goalSelector.addGoal(1, std::make_unique<PanicGoal>(*this, 0.5));
	_goalSelector.addGoal(2, std::make_unique<WanderToPositionGoal>(*this, _wanderTarget));
	_goalSelector.addGoal(4, std::make_unique<MoveTowardsRestrictionGoal>(*this, 0.35));
	_goalSelector.addGoal(8, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 0.35));
	_goalSelector.addGoal(9, std::make_unique<LookAtPlayerGoal>(*this, "Player", 3.0F, 1.0F));
	_goalSelector.addGoal(10, std::make_unique<LookAtPlayerGoal>(*this, "Mob", 8.0F));
}

void WanderingTrader::customServerAiStep() {
	AbstractVillager::customServerAiStep();
	// WanderingTrader.maybeDespawn
	if (_despawnDelay > 0 && --_despawnDelay == 0) {
		discard();
		return;
	}
	// UseItemGoal: an invisibility potion at night (unless invisible already), milk in the day (when invisible)
	if (_drinkTicks > 0) {
		if (--_drinkTicks == 0) {
			if (_drinkingMilk) {
				removeAllEffects();
			} else {
				addEffect({MobEffects::Invisibility, 9600, 0});
			}
			setItemSlot(EquipmentSlot::MainHand, ItemStack{});
			playSound(_drinkingMilk ? "minecraft:entity.wandering_trader.drink_milk" : "minecraft:entity.wandering_trader.drink_potion", 1.0F, 1.0F);
		}
		return;
	}
	bool night = !_level.isBrightOutside();
	bool invisible = hasEffect(MobEffects::Invisibility);
	if ((night && !invisible) || (!night && invisible)) {
		_drinkingMilk = invisible;
		const GameData& data = _level.gameData();
		ItemStack		item(data.getStaticId("minecraft:item", invisible ? "minecraft:milk_bucket" : "minecraft:potion"), 1);
		if (!invisible) Components::set(item, data, "minecraft:potion_contents", Components::encodePotion(data.getStaticId("minecraft:potion", "minecraft:invisibility")));
		setItemSlot(EquipmentSlot::MainHand, item);
		_drinkTicks = 32;
	}
}

void WanderingTrader::updateTrades() { VillagerTrades::addWanderingTraderOffers(_level, _random, _offers); }

void WanderingTrader::save(Buffer& buf) const {
	AbstractVillager::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeInt(_despawnDelay);
	buf.writeBool(_wanderTarget.has_value());
	if (_wanderTarget) buf.writeLong(_wanderTarget->asLong());
}

void WanderingTrader::load(Buffer& buf) {
	AbstractVillager::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown wandering trader data version");
	_despawnDelay = buf.readInt();
	if (buf.readBool()) _wanderTarget = BlockPos::fromLong(buf.readLong());
}

void Villager::die(const Combat::DamageSource& source) {
	int difficulty = _level.difficulty();
	if (dynamic_cast<Zombie*>(source.causing) && difficulty >= 2 && (difficulty == 3 || !source.causing->asLiving()->random().nextBoolean())) {
		int type = _villagerType, profession = _profession, villagerLevel = _villagerLevel, xp = _villagerXp;
		std::vector<MerchantOffer> kept = offers();
		Level& level = _level;
		BlockPos pos = blockPosition();
		Mob* converted = convertTo(level.gameData().getStaticId("minecraft:entity_type", "minecraft:zombie_villager"), false, true, [&](Mob& mob) {
			DifficultyInstance instance = level.getCurrentDifficultyAt(mob.blockPosition());
			mob.finalizeSpawn(instance, static_cast<int>(MobRegistry::SpawnReason::Conversion));
			if (auto* zombie = dynamic_cast<ZombieVillager*>(&mob)) {
				zombie->setVillagerData(type, profession, villagerLevel);
				zombie->setTradeOffers(kept);
				zombie->setVillagerXp(xp);
			}
		});
		if (converted) {
			level.levelEvent(nullptr, 1026, pos, 0); // LevelEvent.SOUND_ZOMBIE_INFECTED
			return;
		}
	}
	AbstractVillager::die(source);
}

bool Villager::wantsToSpawnGolem(int64_t gameTime) {
	// GolemSensor: an iron golem within 16 blocks, sensed every 10 seconds, keeps it quiet for 30 seconds
	if (_lastGolemSense < 0 || gameTime - _lastGolemSense >= 200) {
		_lastGolemSense = gameTime;
		for (Actor* actor : _level.actorsIn(boundingBox().inflate(16.0, 16.0, 16.0), this)) {
			if (dynamic_cast<IronGolem*>(actor) && actor->isAlive()) golemDetected(gameTime);
		}
	}
	if (gameTime < _golemDetectedUntil) return false;
	return _lastSlept >= 0 && gameTime - _lastSlept < 24000;
}

bool Villager::spawnGolemIfNeeded(int64_t gameTime, int minVillagers) {
	if (!wantsToSpawnGolem(gameTime)) return false;
	std::vector<Villager*> around, willing;
	for (Actor* actor : _level.actorsIn(boundingBox().inflate(10.0, 10.0, 10.0))) {
		auto* villager = dynamic_cast<Villager*>(actor);
		if (!villager || !villager->isAlive()) continue;
		around.push_back(villager);
		if (willing.size() < 5 && villager->wantsToSpawnGolem(gameTime)) willing.push_back(villager);
	}
	if (static_cast<int>(willing.size()) < minVillagers) return false;
	// SpawnUtil.trySpawnMob: 10 tries within 8 blocks, from 6 above down to 6 below, on a solid block with air (or
	// liquid) above (LEGACY_IRON_GOLEM)
	int		 golemType = _level.gameData().getStaticId("minecraft:entity_type", "minecraft:iron_golem");
	BlockPos center	   = blockPosition();
	for (int attempt = 0; attempt < 10; attempt++) {
		int dx = _random.nextInt(17) - 8, dz = _random.nextInt(17) - 8;
		for (int y = center.y + 6; y >= center.y - 6; y--) {
			BlockPos at{center.x + dx, y, center.z + dz};
			int		 below = _level.getBlockState(at.below()), here = _level.getBlockState(at), above = _level.getBlockState(at.above());
			const GameData::StateProperties& hereProps = _level.gameData().getStateProperties(here);
			bool open = (_level.blocks().isAir(here) || hereProps.liquid) && (_level.blocks().isAir(above) || _level.gameData().getStateProperties(above).liquid);
			if (!open || !_level.gameData().getStateProperties(below).solidRender) continue;
			std::unique_ptr<Mob> golem = _level.mobs().create(_level, golemType);
			if (!golem) return false;
			golem->snapTo({at.x + 0.5, static_cast<double>(at.y), at.z + 0.5}, 0.0F, 0.0F);
			if (!golem->checkSpawnObstruction()) break;
			DifficultyInstance difficulty = _level.getCurrentDifficultyAt(at);
			golem->finalizeSpawn(difficulty, static_cast<int>(MobRegistry::SpawnReason::MobSummoned));
			_level.addFreshEntity(std::move(golem));
			for (Villager* villager : around) villager->golemDetected(gameTime);
			return true;
		}
	}
	return false;
}
