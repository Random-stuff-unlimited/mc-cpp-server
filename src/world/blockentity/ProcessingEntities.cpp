#include "world/blockentity/ProcessingEntities.hpp"

#include "data/GameData.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/Xp.hpp"
#include "world/item/FuelValues.hpp"
#include "world/item/PotionBrewing.hpp"

#include <algorithm>
#include <cmath>
#include <random>

std::unique_ptr<BlockEntity> createProcessingBlockEntity(const std::string& type, const BlockPos& pos) {
	if (type == "minecraft:furnace" || type == "minecraft:blast_furnace" || type == "minecraft:smoker") {
		return std::make_unique<AbstractFurnaceBlockEntity>(type, pos);
	}
	if (type == "minecraft:brewing_stand") return std::make_unique<BrewingStandBlockEntity>(pos);
	return nullptr;
}

namespace {
	// Item ids these block entities check every tick, looked up once per game data
	struct Ids {
		int bucket, waterBucket, wetSponge, potion, splashPotion, lingeringPotion, glassBottle;
	};
	const Ids& ids(const GameData& gameData) {
		static const GameData* cachedFor = nullptr;
		static Ids			   cached{};
		if (cachedFor != &gameData) {
			auto item = [&](const char* name) { return gameData.getStaticId("minecraft:item", name); };
			cached	  = {item("minecraft:bucket"), item("minecraft:water_bucket"), item("minecraft:wet_sponge"), item("minecraft:potion"),
						 item("minecraft:splash_potion"), item("minecraft:lingering_potion"), item("minecraft:glass_bottle")};
			cachedFor = &gameData;
		}
		return cached;
	}
	int itemMaxStackSize(const GameData& gameData, const ItemStack& stack) {
		const GameData::ItemProperties* item = gameData.getItemProperties(stack.item);
		return item ? item->maxStackSize : 64;
	}
	// Item.getCraftingRemainder: one of the item it leaves (an empty bucket for a lava bucket), or nothing
	ItemStack craftingRemainder(const GameData& gameData, int item) {
		const GameData::ItemProperties* properties = gameData.getItemProperties(item);
		if (!properties || properties->craftingRemainder.empty()) return {};
		int remainder = gameData.getStaticId("minecraft:item", properties->craftingRemainder);
		return remainder > 0 ? ItemStack(remainder, 1) : ItemStack();
	}
	void clearIfEmpty(ItemStack& stack) {
		if (stack.isEmpty()) stack = ItemStack();
	}
	CraftingInput singleInput(const ItemStack& stack) { return CraftingInput::ofPositioned(1, 1, {stack}).input; }
} // namespace

// ===== AbstractFurnaceBlockEntity =====

AbstractFurnaceBlockEntity::AbstractFurnaceBlockEntity(std::string type, const BlockPos& pos)
	: ContainerBlockEntity(std::move(type), pos, 3),
	  _recipeType(this->type() == "minecraft:blast_furnace" ? RecipeType::Blasting
				  : this->type() == "minecraft:smoker"		? RecipeType::Smoking
															: RecipeType::Smelting) {}

std::string AbstractFurnaceBlockEntity::defaultName() const {
	switch (_recipeType) {
	case RecipeType::Blasting:
		return "container.blast_furnace";
	case RecipeType::Smoking:
		return "container.smoker";
	default:
		return "container.furnace";
	}
}

int AbstractFurnaceBlockEntity::data(int index) const {
	switch (index) {
	case 0:
		return litTimeRemaining;
	case 1:
		return litTotalTime;
	case 2:
		return cookingTimer;
	case 3:
		return cookingTotalTime;
	default:
		return 0;
	}
}

// BlastFurnaceBlockEntity and SmokerBlockEntity burn their fuel twice as fast
int AbstractFurnaceBlockEntity::burnDuration(Level& level, const ItemStack& fuel) const {
	int duration = level.fuelValues().burnDuration(fuel);
	return _recipeType == RecipeType::Smelting ? duration : duration / 2;
}

const Recipe* AbstractFurnaceBlockEntity::recipeFor(Level& level, const ItemStack& input) const {
	const Recipe* recipe = level.recipes().getRecipeFor(_recipeType, singleInput(input), _lastRecipe);
	if (recipe) _lastRecipe = recipe;
	return recipe;
}

// getTotalCookTime: the recipe's cooking time, 200 without one
int AbstractFurnaceBlockEntity::totalCookTime(Level& level) const {
	const Recipe* recipe = recipeFor(level, _items[SLOT_INPUT]);
	return recipe ? recipe->cookingTime : BURN_TIME_STANDARD;
}

// canBurn: a result, and room for it in the result slot
bool AbstractFurnaceBlockEntity::canBurn(Level& level, const Recipe* recipe, const ItemStack& input) const {
	if (input.isEmpty() || !recipe) return false;
	ItemStack result = recipe->assemble(singleInput(input));
	if (result.isEmpty()) return false;
	const ItemStack& there = _items[SLOT_RESULT];
	if (there.isEmpty()) return true;
	if (!there.sameItemSameComponents(result)) return false;
	const GameData& gameData = level.gameData();
	if (there.count < maxStackSize() && there.count < itemMaxStackSize(gameData, there)) return true;
	return there.count < itemMaxStackSize(gameData, result);
}

// burn: one input cooked into the result slot
bool AbstractFurnaceBlockEntity::burn(Level& level, const Recipe* recipe, const ItemStack& input) {
	if (!recipe || !canBurn(level, recipe, input)) return false;
	const Ids& id	  = ids(level.gameData());
	ItemStack  result = recipe->assemble(singleInput(input));
	ItemStack& there  = _items[SLOT_RESULT];
	if (there.isEmpty()) {
		there = result;
	} else if (there.sameItemSameComponents(result)) {
		there.grow(1);
	}
	// A wet sponge dries into the bucket of the fuel slot
	ItemStack& fuel = _items[SLOT_FUEL];
	if (_items[SLOT_INPUT].item == id.wetSponge && !fuel.isEmpty() && fuel.item == id.bucket) fuel = ItemStack(id.waterBucket, 1);
	_items[SLOT_INPUT].shrink(1);
	clearIfEmpty(_items[SLOT_INPUT]);
	return true;
}

// AbstractFurnaceBlockEntity.serverTick
void AbstractFurnaceBlockEntity::tick(Level& level) {
	bool wasLit	 = isLit();
	bool changed = false;
	if (isLit()) litTimeRemaining--;

	ItemStack& fuel		= _items[SLOT_FUEL];
	ItemStack& input	= _items[SLOT_INPUT];
	bool	   hasInput = !input.isEmpty();
	bool	   hasFuel	= !fuel.isEmpty();
	if (isLit() || (hasFuel && hasInput)) {
		const Recipe* recipe = hasInput ? recipeFor(level, input) : nullptr;
		if (!isLit() && canBurn(level, recipe, input)) {
			litTimeRemaining = burnDuration(level, fuel);
			litTotalTime	 = litTimeRemaining;
			if (isLit()) {
				changed = true;
				if (hasFuel) {
					// The fuel's remainder (a lava bucket leaves its bucket) once the last one burns
					int item = fuel.item;
					fuel.shrink(1);
					if (fuel.isEmpty()) fuel = craftingRemainder(level.gameData(), item);
				}
			}
		}
		if (isLit() && canBurn(level, recipe, input)) {
			cookingTimer++;
			if (cookingTimer == cookingTotalTime) {
				cookingTimer	 = 0;
				cookingTotalTime = totalCookTime(level);
				if (burn(level, recipe, input)) setRecipeUsed(recipe);
				changed = true;
			}
		} else {
			cookingTimer = 0;
		}
	} else if (!isLit() && cookingTimer > 0) {
		// Cools down twice as fast as it cooks
		cookingTimer = std::clamp(cookingTimer - BURN_COOL_SPEED, 0, std::max(cookingTotalTime, 0));
	}

	if (wasLit != isLit()) {
		changed	  = true;
		int state = level.getBlockState(pos());
		int lit	  = level.blocks().property("lit");
		if (level.blocks().has(state, lit)) level.setBlock(pos(), level.blocks().withBool(state, lit, isLit()), Level::UPDATE_ALL);
	}
	if (changed) markChanged();
}

void AbstractFurnaceBlockEntity::setItem(int slot, ItemStack stack) {
	const ItemStack& old  = _items.at(slot);
	bool			 same = !stack.isEmpty() && old.sameItemSameComponents(stack);
	ContainerBlockEntity::setItem(slot, std::move(stack));
	// Another input: its cooking starts over
	if (slot == SLOT_INPUT && !same && level()) {
		cookingTotalTime = totalCookTime(*level());
		cookingTimer	 = 0;
		markChanged();
	}
}

bool AbstractFurnaceBlockEntity::canPlaceItem(int slot, const ItemStack& stack) const {
	if (slot == SLOT_RESULT) return false;
	if (slot != SLOT_FUEL) return true;
	// A fuel, or one empty bucket (for a wet sponge to fill)
	if (!level()) return false;
	const Ids& id = ids(level()->gameData());
	return level()->fuelValues().isFuel(stack) || (stack.item == id.bucket && _items[SLOT_FUEL].item != id.bucket);
}

std::vector<int> AbstractFurnaceBlockEntity::slotsForFace(Direction face) {
	if (face == Direction::Down) return {SLOT_RESULT, SLOT_FUEL};
	return face == Direction::Up ? std::vector<int>{SLOT_INPUT} : std::vector<int>{SLOT_FUEL};
}

// Only the buckets leave the fuel slot through the bottom
bool AbstractFurnaceBlockEntity::canTakeItemThroughFace(int slot, const ItemStack& stack, Direction face) {
	if (face != Direction::Down || slot != SLOT_FUEL) return true;
	if (!level()) return false;
	const Ids& id = ids(level()->gameData());
	return stack.item == id.waterBucket || stack.item == id.bucket;
}

void AbstractFurnaceBlockEntity::setRecipeUsed(const Recipe* recipe) {
	if (recipe) recipesUsed[recipe->id]++;
}

// createExperience for each recipe: floor(times x experience), one more with the chance of the fraction left
int AbstractFurnaceBlockEntity::experienceToAward(Level& level) const {
	static std::mt19937 random(std::random_device{}()); // Math.random
	int					total = 0;
	for (const auto& [id, times] : recipesUsed) {
		const Recipe* recipe = level.recipes().byId(id);
		if (!recipe) continue;
		float amount = static_cast<float>(times) * recipe->experience;
		int	  whole	 = static_cast<int>(std::floor(amount));
		float frac	 = amount - static_cast<float>(whole);
		if (frac != 0.0F && std::uniform_real_distribution<double>(0.0, 1.0)(random) < frac) whole++;
		total += whole;
	}
	return total;
}

// Vanilla gives the recipes (every player knows them all here), pops the experience and forgets the recipes used.
// FurnaceResultSlot.checkTakeAchievements: the smelted XP goes to the player taking the result
void AbstractFurnaceBlockEntity::awardUsedRecipesAndPopExperience(Player& player) {
	if (!level()) return;
	if (int xp = experienceToAward(*level()); xp > 0) Xp::addExperience(level()->server(), player, xp);
	recipesUsed.clear();
	markChanged();
}

void AbstractFurnaceBlockEntity::preRemoveSideEffects(Level& level) {
	ContainerBlockEntity::preRemoveSideEffects(level);
	// getRecipesToAwardAndPopExperience at the block's center: no experience orbs yet, the experience is lost
}

// After the items and custom name: cooking_time_spent, cooking_total_time, lit_time_remaining, lit_total_time (shorts),
// then RecipesUsed (recipe id -> count)
void AbstractFurnaceBlockEntity::saveExtra(BlockEntityWriter& out) const {
	for (int value : {cookingTimer, cookingTotalTime, litTimeRemaining, litTotalTime}) out.varint(static_cast<uint16_t>(value));
	out.varint(static_cast<uint32_t>(recipesUsed.size()));
	for (const auto& [id, times] : recipesUsed) {
		out.string(id);
		out.varint(static_cast<uint32_t>(times));
	}
}

void AbstractFurnaceBlockEntity::loadExtra(BlockEntityReader& in) {
	cookingTimer	 = static_cast<int16_t>(in.varint());
	cookingTotalTime = static_cast<int16_t>(in.varint());
	litTimeRemaining = static_cast<int16_t>(in.varint());
	litTotalTime	 = static_cast<int16_t>(in.varint());
	recipesUsed.clear();
	uint32_t count = in.varint();
	for (uint32_t i = 0; i < count; i++) {
		std::string id = in.string();
		recipesUsed[id] = static_cast<int>(in.varint());
	}
}

// ===== BrewingStandBlockEntity =====

namespace {
	bool isBrewingFuel(const GameData& gameData, const ItemStack& stack) {
		return !stack.isEmpty() && gameData.isInTag("minecraft:item", "minecraft:brewing_fuel", stack.item);
	}
} // namespace

// BrewingStandBlockEntity.serverTick
void BrewingStandBlockEntity::tick(Level& level) {
	ItemStack& fuelStack = _items[FUEL_SLOT];
	if (fuel <= 0 && isBrewingFuel(level.gameData(), fuelStack)) {
		fuel = FUEL_USES;
		fuelStack.shrink(1);
		clearIfEmpty(fuelStack);
		markChanged();
	}

	bool brewable	= isBrewable(level);
	bool brewing	= brewTime > 0;
	int	 ingredient = _items[INGREDIENT_SLOT].isEmpty() ? 0 : _items[INGREDIENT_SLOT].item;
	if (brewing) {
		brewTime--;
		bool done = brewTime == 0;
		if (done && brewable) {
			doBrew(level);
		} else if (!brewable || ingredient != _ingredient) {
			// The ingredient changed: the brew stops
			brewTime = 0;
		}
		markChanged();
	} else if (brewable && fuel > 0) {
		fuel--;
		brewTime	= BREW_TIME;
		_ingredient = ingredient;
		markChanged();
	}

	// The bottles the block shows
	std::array<bool, 3> bits{};
	for (int i = 0; i < 3; i++) bits[i] = !_items[i].isEmpty();
	if (_lastPotionCount != bits) {
		_lastPotionCount = bits;
		int state		 = level.getBlockState(pos());
		if (!level.gameData().isInstanceOf(level.blocks().blockOf(state), "BrewingStandBlock")) return;
		for (int i = 0; i < 3; i++) state = level.blocks().withBool(state, level.blocks().property("has_bottle_" + std::to_string(i)), bits[i]);
		level.setBlock(pos(), state, Level::UPDATE_CLIENTS);
	}
}

// isBrewable: an ingredient that does something to one of the bottles
bool BrewingStandBlockEntity::isBrewable(Level& level) const {
	const ItemStack&	 ingredient = _items[INGREDIENT_SLOT];
	const PotionBrewing& brewing	= level.potionBrewing();
	if (ingredient.isEmpty() || !brewing.isIngredient(ingredient)) return false;
	for (int i = 0; i < 3; i++) {
		if (!_items[i].isEmpty() && brewing.hasMix(_items[i], ingredient)) return true;
	}
	return false;
}

void BrewingStandBlockEntity::doBrew(Level& level) {
	ItemStack			 ingredient = _items[INGREDIENT_SLOT];
	const PotionBrewing& brewing	= level.potionBrewing();
	for (int i = 0; i < 3; i++) _items[i] = brewing.mix(ingredient, _items[i]);
	ingredient.shrink(1);
	// Like vanilla, the remainder is the one of the stack's item after it shrank: none once the last one is used
	ItemStack remainder = ingredient.isEmpty() ? ItemStack() : craftingRemainder(level.gameData(), ingredient.item);
	if (!remainder.isEmpty()) {
		if (ingredient.isEmpty()) {
			ingredient = remainder;
		} else {
			dropItemStack(level, pos().x, pos().y, pos().z, remainder);
		}
	}
	clearIfEmpty(ingredient);
	_items[INGREDIENT_SLOT] = ingredient;
	level.levelEvent(nullptr, 1035, pos(), 0); // LevelEvent.SOUND_BREWING_STAND_BREW
}

bool BrewingStandBlockEntity::canPlaceItem(int slot, const ItemStack& stack) const {
	if (!level()) return false;
	if (slot == INGREDIENT_SLOT) return level()->potionBrewing().isIngredient(stack);
	if (slot == FUEL_SLOT) return isBrewingFuel(level()->gameData(), stack);
	const Ids& id = ids(level()->gameData());
	bool	   bottle = stack.item == id.potion || stack.item == id.splashPotion || stack.item == id.lingeringPotion || stack.item == id.glassBottle;
	return !stack.isEmpty() && bottle && _items.at(slot).isEmpty();
}

std::vector<int> BrewingStandBlockEntity::slotsForFace(Direction face) {
	if (face == Direction::Up) return {INGREDIENT_SLOT};
	return face == Direction::Down ? std::vector<int>{0, 1, 2, INGREDIENT_SLOT} : std::vector<int>{0, 1, 2, FUEL_SLOT};
}

// Through the bottom, the ingredient slot only gives a glass bottle (the remainder of a dragon's breath)
bool BrewingStandBlockEntity::canTakeItemThroughFace(int slot, const ItemStack& stack, Direction) {
	if (slot != INGREDIENT_SLOT) return true;
	return level() && stack.item == ids(level()->gameData()).glassBottle;
}

// After the items and custom name: BrewTime (short), Fuel (byte)
void BrewingStandBlockEntity::saveExtra(BlockEntityWriter& out) const {
	out.varint(static_cast<uint16_t>(brewTime));
	out.u8(static_cast<uint8_t>(fuel));
}

void BrewingStandBlockEntity::loadExtra(BlockEntityReader& in) {
	brewTime = static_cast<int16_t>(in.varint());
	if (brewTime > 0) _ingredient = _items[INGREDIENT_SLOT].isEmpty() ? 0 : _items[INGREDIENT_SLOT].item;
	fuel = static_cast<int8_t>(in.u8());
}
