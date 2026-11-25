#ifndef VILLAGERS_HPP
#define VILLAGERS_HPP

#include "world/entity/Trading.hpp"
#include "world/entity/mobs/Animal.hpp"

#include <optional>

// AbstractVillager: villagers and wandering traders, merchants. A click opens its trades (the merchant menu) when it
// has some, else it shakes its head (its unhappy counter, on the clients)
class AbstractVillager : public AgeableMob {
  public:
	static constexpr int DATA_UNHAPPY_COUNTER = 16;

	AbstractVillager(Level& level, int typeId);

	int	 unhappyCounter() const { return _unhappy; }
	void setUnhappy();

	// ----- Merchant -----

	// Its offers, made the first time they are asked for (updateTrades)
	std::vector<MerchantOffer>& offers();
	// The player trading with it, null if none
	Player*						tradingPlayer();
	void						setTradingPlayer(Player* player);
	bool						isTrading() { return tradingPlayer() != nullptr; }
	// AbstractVillager.notifyTrade: an offer was used (its experience, the merchant's reaction)
	virtual void				notifyTrade(MerchantOffer& offer);
	virtual int					merchantLevel() const { return 0; }
	virtual int					merchantXp() const { return 0; }
	virtual bool				showProgressBar() const { return false; }
	virtual bool				canRestock() const { return false; }
	// The sound of a trade, and of an item put in a payment slot (yes/no)
	virtual std::string			tradeSound() const = 0;
	virtual std::string			notifyTradeSound(bool yes) const = 0;
	// The mobs villagers flee (VillagerHostilesSensor: zombies, illagers, ravagers, vexes, zoglins)
	static bool isHostile(Level& level, Actor& actor);
	using AgeableMob::playSound;

	bool mobInteract(Player& player, int hand) override;
	bool removeWhenFarAway(double) const override { return false; }
	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;

  protected:
	std::vector<MerchantOffer> _offers;

	// The offers it gets (a villager: two of its level; a wandering trader: its six)
	virtual void updateTrades() = 0;
	void		 customServerAiStep() override;
	void		 writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	int		  _unhappy = 0;
	EntityRef _tradingPlayer;
};

// Villager: it finds a job (the nearest free job site block gives its profession) and a bed, and lives by the clock:
// work at its job site in the day, a meeting at the bell in the afternoon, sleep in its bed at night. It flees
// zombies and illagers, picks up food, and two villagers with enough food and a free bed make a baby. Gossip,
// farming, iron golems and trading aren't ported
class Villager : public AbstractVillager {
  public:
	static constexpr int DATA_VILLAGER_DATA = 17, DATA_SLEEPING_POS = 14;
	// Zombie.killedEntity: killed by a zombie in normal (one time in two) or hard, it becomes a zombie villager
	void die(const Combat::DamageSource& source) override;
	void setVillagerXp(int xp) { _villagerXp = xp; }
	// Villager.wantsToSpawnGolem: slept within the last day and no iron golem seen around lately (GolemSensor)
	bool wantsToSpawnGolem(int64_t gameTime);
	// Villager.spawnGolemIfNeeded: with enough villagers around that want one too (5 when gossiping, 3 when
	// panicking), an iron golem appears near it (SpawnUtil.trySpawnMob, LEGACY_IRON_GOLEM). Whether one did
	bool spawnGolemIfNeeded(int64_t gameTime, int minVillagers);
	void golemDetected(int64_t gameTime) { _golemDetectedUntil = gameTime + 600; }
	void setLastSlept(int64_t gameTime) { _lastSlept = gameTime; }
	// Its inventory (vanilla's 8 slots): food, and a farmer's seeds and wheat
	std::vector<ItemStack>& villagerInventory() { return _food; }
	void					addToInventory(int item, int count) { addFood(item, count); }
	// Takes one of these items out of its inventory; whether it had one
	bool					takeOne(int item);
	// WorkAtComposter: bakes bread (3 wheat each) and composts the seeds beyond 10 into its composter
	void					workAtComposter(const BlockPos& composter);
	// Villager.thunderHit: a witch (not in peaceful)
	void thunderHit(LightningBolt& bolt) override;
	static constexpr int POSE_SLEEPING = 2;
	static constexpr int BREEDING_FOOD = 12;
	// VillagerSchedule's activities
	enum class Activity { Idle, Work, Meet, Rest, Play };

	Villager(Level& level, int typeId);

	int	 villagerType() const { return _villagerType; }
	int	 profession() const { return _profession; }
	int	 villagerLevel() const { return _villagerLevel; }
	void setVillagerData(int type, int profession, int level);
	// The profession's name ("minecraft:farmer"), and whether it has one
	std::string professionName() const;
	bool		hasJob() const;
	bool		isNitwit() const { return professionName() == "minecraft:nitwit"; }
	// VillagerSchedule.DEFAULT / SIMPLE (babies): what it does at this time of day
	Activity	activity() const;

	const std::optional<BlockPos>& jobSite() const { return _jobSite; }
	const std::optional<BlockPos>& home() const { return _home; }
	const std::optional<BlockPos>& meetingPoint() const { return _meetingPoint; }
	bool						   isSleeping() const { return _sleeping.has_value(); }
	void						   startSleeping(const BlockPos& bed);
	void						   stopSleeping();
	// The food it carries, in food points (Villager.FOOD_POINTS: bread 4, carrots, potatoes and beetroots 1)
	int							   foodPoints() const;
	void						   eatFoodPoints(int points);
	void						   addFood(int item, int count);

	void							registerGoals() override;
	void							notifyTrade(MerchantOffer& offer) override;
	int								merchantLevel() const override { return _villagerLevel; }
	int								merchantXp() const override { return _villagerXp; }
	bool							showProgressBar() const override { return true; }
	bool							canRestock() const override { return true; }
	std::string						tradeSound() const override { return "minecraft:entity.villager.trade"; }
	std::string						notifyTradeSound(bool yes) const override { return yes ? "minecraft:entity.villager.yes" : "minecraft:entity.villager.no"; }
	// Villager.restock: its offers in stock again (twice a day at its job site)
	void							restock();
	bool							hurtServer(const Combat::DamageSource& source, float amount) override;
	bool							wantsToPickUp(const ItemStack& stack) override;
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	std::string						ambientSoundName() override;
	void							save(Buffer& buf) const override;
	void							load(Buffer& buf) override;

  protected:
	void customServerAiStep() override;
	void pickUpItem(ItemEntity& item) override;
	void dropAllDeathLoot(const Combat::DamageSource& source) override;
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	int						_villagerType = 0, _profession = 0, _villagerLevel = 1, _villagerXp = 0;
	int						_updateMerchantTimer = 0, _restocksToday = 0;
	int64_t					_lastSlept = -1, _lastGossip = -1, _golemDetectedUntil = -1, _lastGolemSense = -1;
	int64_t					_lastRestockGameTime = 0, _lastRestockCheckDay = -1;
	bool					_increaseProfessionLevelOnUpdate = false;
	std::optional<BlockPos> _jobSite, _home, _meetingPoint, _sleeping;
	std::vector<ItemStack>	_food;
	int						_lookCooldown = 0, _breedCooldown = 0;

	void updateTrades() override;
	void increaseMerchantCareer();
	void acquirePoi();
	void releasePoi(std::optional<BlockPos>& pos);
	void tryBreed();
	void setBedOccupied(const BlockPos& bed, bool occupied);
};

// WanderingTrader: comes and goes (despawns after 40 minutes), wanders toward a spot, drinks an invisibility potion at
// night and milk in the morning, flees zombies and illagers. Its trades and its spawning near players aren't ported
class WanderingTrader : public AbstractVillager {
  public:
	WanderingTrader(Level& level, int typeId);

	int	 despawnDelay() const { return _despawnDelay; }
	void setDespawnDelay(int ticks) { _despawnDelay = ticks; }
	void setWanderTarget(const BlockPos& pos) { _wanderTarget = pos; }

	void		registerGoals() override;
	std::string tradeSound() const override { return "minecraft:entity.wandering_trader.trade"; }
	std::string notifyTradeSound(bool yes) const override { return yes ? "minecraft:entity.wandering_trader.yes" : "minecraft:entity.wandering_trader.no"; }
	std::string ambientSoundName() override { return "minecraft:entity.wandering_trader.ambient"; }
	void		save(Buffer& buf) const override;
	void		load(Buffer& buf) override;

  protected:
	void customServerAiStep() override;
	void updateTrades() override;

  private:
	int						_despawnDelay = 48000, _drinkTicks = 0;
	std::optional<BlockPos> _wanderTarget;
	bool					_drinkingMilk = false;
};

#endif
