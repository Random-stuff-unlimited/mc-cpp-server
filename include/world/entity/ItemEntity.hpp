#ifndef ITEM_ENTITY_HPP
#define ITEM_ENTITY_HPP

#include "world/entity/Entity.hpp"
#include "world/item/ItemStack.hpp"

#include <memory>

class Player;

// An item lying in the world (vanilla's ItemEntity)
class ItemEntity : public Entity {
  public:
	static constexpr int INFINITE_PICKUP_DELAY = 32767;
	static constexpr int INFINITE_LIFETIME	   = -32768;
	static constexpr int LIFETIME			   = 6000;

	ItemEntity(Level& level, const Vec3& position, ItemStack stack, const Vec3& delta);
	// With vanilla's random initial push (the ItemEntity(level, x, y, z, stack) constructor)
	static std::unique_ptr<ItemEntity> create(Level& level, const Vec3& position, ItemStack stack);

	const ItemStack& item() const { return _stack; }
	void			 setItem(ItemStack stack);
	int				 pickupDelay() const { return _pickupDelay; }
	void			 setPickupDelay(int delay) { _pickupDelay = delay; }
	int				 age() const { return _age; }

	void tick() override;
	// ItemEntity.playerTouch: picked up if the delay is over and it fits in the inventory. Returns the count taken
	// for the pickup animation, 0 if none
	int	 playerTouch(Player& player);
	void writeEntityData(Buffer& buf) const override;
	// For a copy (dimension travel): not saved with the chunks
	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;

  protected:
	BlockPos blockPosBelowAffectingMovement() override { return getOnPos(0.999999f); }

  private:
	ItemStack _stack;
	int		  _age		   = 0;
	int		  _pickupDelay = 0;
	int		  _health	   = 5;

	bool isMergable() const;
	void mergeWithNeighbours();
	void tryToMerge(ItemEntity& other);
	void setFluidMovement(double factor);
	// applyEffectsFromBlocks: lava, fire and cactus hurt items
	void applyEffectsFromBlocks();
	void hurt(float amount);
	bool fireImmune() const;
	int	 maxStackSize(const ItemStack& stack) const;
};

#endif
