#ifndef HELPER_MOBS_HPP
#define HELPER_MOBS_HPP

#include "world/entity/mobs/Animal.hpp"

// Allay: a little flying spirit that likes the player who gives it an item: it picks up the matching items around (up
// to a stack) and brings them to that player, staying close. Its player can't hurt it, and gets the item back by
// taking it from its hand. Dancing to jukeboxes, duplicating and note blocks aren't ported
class Allay : public Mob {
  public:
	static constexpr int DATA_DANCING = 16, DATA_CAN_DUPLICATE = 17;
	static constexpr int THROW_COOLDOWN = 60;

	Allay(Level& level, int typeId);

	Player*			 likedPlayer();
	const ItemStack& inventory() const { return _inventory; }
	ItemStack&		 inventoryMutable() { return _inventory; }
	bool			 hasItemInHand() { return !getMainHandItem().isEmpty(); }
	// Allay.wantsToPickUp: the item it holds, while its inventory has room
	bool			 wantsItem(const ItemStack& stack);
	// Puts what it can of the stack into its inventory; the count taken
	int				 addToInventory(const ItemStack& stack);
	int				 throwCooldown = 0;
	using Mob::playSound;
	// Dancing by a playing jukebox (within 10 blocks); an amethyst shard given while dancing makes a second allay
	// (then 5 minutes before it can again)
	bool			 isDancing() const { return _dancing; }
	bool			 canDuplicate() const { return _duplicationCooldown == 0; }

	void		registerGoals() override;
	bool		mobInteract(Player& player, int hand) override;
	bool		hurtServer(const Combat::DamageSource& source, float amount) override;
	bool		removeWhenFarAway(double) const override { return false; }
	bool		onClimbable() override { return false; }
	std::string ambientSoundName() override { return hasItemInHand() ? "minecraft:entity.allay.ambient_with_item" : "minecraft:entity.allay.ambient_without_item"; }
	void		save(Buffer& buf) const override;
	void		load(Buffer& buf) override;

  protected:
	std::unique_ptr<PathNavigation> createNavigation() override;
	void							customServerAiStep() override;
	void							checkFallDamage(double, bool, const BlockPos&) override {}
	void							dropAllDeathLoot(const Combat::DamageSource& source) override;
	void							writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	ItemStack _inventory;
	EntityRef _liked;
	bool	  _dancing = false;
	int		  _duplicationCooldown = 0;

	void setDancing(bool dancing);
	void setDuplicationCooldown(int ticks);
	bool jukeboxNearby();
};

// CopperGolem: a golem of copper that sorts: it takes up to 16 items from a copper chest nearby and puts them into a
// chest that already holds that item (else an empty one), then goes back for more. It oxidizes over time (an axe
// scrapes it, honeycomb waxes it). Its oxidized statue form and the poppy on its head aren't ported
class CopperGolem : public Mob {
  public:
	static constexpr int DATA_WEATHER_STATE = 16, DATA_COPPER_GOLEM_STATE = 17;
	// WeatheringCopper.WeatherState and CopperGolemState
	enum class Weather : uint8_t { Unaffected = 0, Exposed = 1, Weathered = 2, Oxidized = 3 };
	enum class State : uint8_t { Idle = 0, GettingItem = 1, GettingNoItem = 2, DroppingItem = 3, DroppingNoItem = 4 };
	static constexpr int MAX_CARRIED = 16, INTERACT_TICKS = 60, SEARCH_RANGE = 32, SEARCH_HEIGHT = 8;

	CopperGolem(Level& level, int typeId);

	Weather weather() const { return _weather; }
	void	setWeather(Weather weather);
	bool	isWaxed() const { return _waxed; }
	void	setWaxed(bool waxed) { _waxed = waxed; }
	State	golemState() const { return _state; }
	void	setGolemState(State state);
	// The chests it uses: the copper chests it takes from, the plain chests it fills
	bool	isSourceChest(const BlockPos& pos);
	bool	isDestinationChest(const BlockPos& pos);
	using Mob::playSound;

	void		registerGoals() override;
	bool		mobInteract(Player& player, int hand) override;
	bool		removeWhenFarAway(double) const override { return false; }
	std::string ambientSoundName() override { return ""; }
	void		save(Buffer& buf) const override;
	void		load(Buffer& buf) override;

  protected:
	void customServerAiStep() override;
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	Weather _weather = Weather::Unaffected;
	State	_state	 = State::Idle;
	bool	_waxed	 = false;
	int64_t _nextWeatheringTick = -1;
};

// HappyGhast: a friendly ghast that floats about calmly; harnessed, players ride it through the air. Ghastlings (its
// babies) grow up faster eating snowballs. Several riders, staying still under a player and leashing boats aren't
// ported
class HappyGhast : public Animal {
  public:
	// HappyGhast: four riders on its harness, at its sides, the first one steers
	int	 maxPassengers() const override { return 4; }
	Vec3 passengerOffset(int index, int) const override {
		static const Vec3 SEATS[4] = {{0.0, 0.0, 0.62}, {-0.62, 0.0, 0.0}, {0.0, 0.0, -0.62}, {0.62, 0.0, 0.0}};
		return SEATS[index & 3];
	}
	static constexpr int DATA_IS_LEASH_HOLDER = 17, DATA_STAYS_STILL = 18;

	HappyGhast(Level& level, int typeId);

	bool isHarnessed() const;

	void		registerGoals() override;
	bool		mobInteract(Player& player, int hand) override;
	bool		isFood(const ItemStack& stack) const override;
	bool		canMate(Animal&) override { return false; }
	bool		isControlledByRider() override { return isHarnessed() && rider() != nullptr; }
	bool		onClimbable() override { return false; }
	std::string ambientSoundName() override { return isBaby() ? "minecraft:entity.ghastling.ambient" : "minecraft:entity.happy_ghast.ambient"; }
	using Animal::playSound;

  protected:
	std::unique_ptr<PathNavigation> createNavigation() override;
	void							travel(const Vec3& input) override;
	void							checkFallDamage(double, bool, const BlockPos&) override {}
	float							babyEyeHeight() const override { return _type.eyeHeight * 0.2375F; }
};

#endif
