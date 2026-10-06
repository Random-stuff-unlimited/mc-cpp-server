#ifndef FARM_ANIMALS_HPP
#define FARM_ANIMALS_HPP

#include "world/entity/mobs/Animal.hpp"

// An animal with a variant from a synced registry (cow, pig, chicken: temperate, warm or cold, by the biome where it
// spawns; a baby takes one parent's)
class VariantAnimal : public Animal {
  public:
	VariantAnimal(Level& level, int typeId, std::string registry, int dataId, int serializer);

	int	 variant() const { return _variant; }
	void setVariant(int variant);

	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	std::unique_ptr<Mob>			getBreedOffspring(Animal& partner) override;
	std::string						lootComponent(const std::string& component) const override;
	void							save(Buffer& buf) const override;
	void							load(Buffer& buf) override;

  protected:
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	std::string _registry; // "minecraft:cow_variant"
	int			_dataId, _serializer;
	int			_variant, _defaultVariant;
};

// Cow (and AbstractCow): milked with a bucket
class Cow : public VariantAnimal {
  public:
	Cow(Level& level, int typeId);
	void registerGoals() override;
	bool mobInteract(Player& player, int hand) override;

  protected:
	float babyEyeHeight() const override { return 0.665F; }
};

// MushroomCow: a cow of the mushroom fields, red or brown (lightning swaps them). A bowl gives mushroom stew (a
// suspicious stew after a brown one ate a flower), shears turn it into a cow and drop its mushrooms
class Mooshroom : public Animal {
  public:
	static constexpr int DATA_TYPE = 17;
	enum Variant { Red = 0, Brown = 1 };

	Mooshroom(Level& level, int typeId) : Animal(level, typeId) {}

	int	 variant() const { return _variant; }
	void setVariant(int variant);
	bool readyForShearing() const { return isAlive() && !isBaby(); }
	// MushroomCow.shear: a cow takes its place, its mushrooms drop
	void shear();

	void				 registerGoals() override;
	bool				 mobInteract(Player& player, int hand) override;
	float				 getWalkTargetValue(const BlockPos& pos) override;
	std::unique_ptr<Mob> getBreedOffspring(Animal& partner) override;
	std::string			 lootComponent(const std::string& component) const override;
	void				 save(Buffer& buf) const override;
	void				 load(Buffer& buf) override;

  protected:
	float babyEyeHeight() const override { return 0.665F; }
	void  writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	int							   _variant = Red;
	std::vector<std::pair<int, int>> _stewEffects; // The flower it ate (empty: none)
	bool						   _hasStewEffects = false;
};

class Pig : public VariantAnimal {
  public:
	Pig(Level& level, int typeId);
	void registerGoals() override;
	// A saddle put on it, then ridden; a carrot on a stick steers it
	bool mobInteract(Player& player, int hand) override;
	bool isControlledByRider() override;
};

// Chicken: lays an egg every 5 to 10 minutes (its variant's), falls slowly
class Chicken : public VariantAnimal {
  public:
	Chicken(Level& level, int typeId);
	void registerGoals() override;
	int	 eggTime() const { return _eggTime; }
	void setEggTime(int ticks) { _eggTime = ticks; }
	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;

  protected:
	void  aiStep() override;
	float babyEyeHeight() const override { return 0.2975F; }

  private:
	int	 _eggTime;
	bool _isChickenJockey = false;
};

// Sheep: its wool (a color, sheared or not), shorn with shears, regrown by eating grass
class Sheep : public Animal {
  public:
	static constexpr int DATA_WOOL = 17; // Color in the low 4 bits, 16: sheared

	Sheep(Level& level, int typeId) : Animal(level, typeId) {}

	int	 color() const { return _wool & 15; }
	void setColor(int color);
	bool isSheared() const { return _wool & 16; }
	void setSheared(bool sheared);
	bool readyForShearing() const { return isAlive() && !isSheared() && !isBaby(); }
	// Sheep.shear: its wool on the ground (1 to 3 of its color)
	void shear();

	void							registerGoals() override;
	bool							mobInteract(Player& player, int hand) override;
	void							ate() override;
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	std::unique_ptr<Mob>			getBreedOffspring(Animal& partner) override;
	std::string						lootComponent(const std::string& component) const override;
	bool							lootTypeSpecific(const nlohmann::json& predicate) const override;
	void							save(Buffer& buf) const override;
	void							load(Buffer& buf) override;

  protected:
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	uint8_t		  _wool = 0;
	EatBlockGoal* _eatBlockGoal = nullptr;
};

// DyeColor's names, by id
const char* dyeColorName(int color);
// DyeColor.getMixedColor: the color the two dyes craft into, else one of them at random
int			mixedDyeColor(Level& level, int a, int b);
// The color of a dye item (DyeItem.getDyeColor), -1 if it isn't one
int			dyeColorOfItem(Level& level, const ItemStack& stack);

#endif
