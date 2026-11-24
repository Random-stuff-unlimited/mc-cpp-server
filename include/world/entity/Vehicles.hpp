#ifndef VEHICLES_HPP
#define VEHICLES_HPP

#include "world/entity/Entity.hpp"
#include "world/item/ItemStack.hpp"

#include <memory>
#include <string>

class SimpleContainer;

// VehicleEntity: boats and minecarts. Hits add damage (10 a point of the hit, healing back 1 a tick) and make it
// wobble; past 40 it breaks and drops its item (nothing for a creative player, who breaks it at once)
class VehicleEntity : public Entity {
  public:
	static constexpr int DATA_HURT = 8, DATA_HURTDIR = 9, DATA_DAMAGE = 10;

	VehicleEntity(Level& level, int typeId, float width, float height) : Entity(level, typeId, width, height) {}

	float damage() const { return _damage; }
	bool  hurtServer(const Combat::DamageSource& source, float amount) override;
	bool  shouldBeSaved() const override { return true; }
	bool  isPushable() const override { return true; }

  protected:
	// VehicleEntity.getDropItem: the item it breaks into
	virtual ItemStack dropItem() const = 0;
	// destroy: its item (and what it holds) at its place, then gone
	virtual void destroy(bool drop);
	// The hurt wobble and the damage, healing over time (Boat/AbstractMinecart.tick)
	void		 tickHurt();
	void		 writeVehicleData(Buffer& buf) const;
	void		 saveVehicle(Buffer& buf) const;
	void		 loadVehicle(Buffer& buf);
	ItemStack	 itemNamed(const std::string& name) const;

	int	  _hurtTime = 0, _hurtDir = 1;
	float _damage	= 0.0F;
};

// AbstractBoat (Boat, ChestBoat, Raft): floats on water and drifts, falls on land; a player sits in it and steers it
// from its client (MOVE_VEHICLE), paddling (PADDLE_BOAT). A chest boat holds 27 items (sneak-click to open it)
class Boat : public VehicleEntity {
  public:
	static constexpr int DATA_PADDLE_LEFT = 11, DATA_PADDLE_RIGHT = 12, DATA_BUBBLE_TIME = 13;

	// At a position, facing yRot (its type: "minecraft:oak_boat", "minecraft:bamboo_chest_raft"...)
	Boat(Level& level, int typeId, const Vec3& position, float yRot);
	Boat(Level& level, int typeId) : Boat(level, typeId, Vec3{}, 0.0F) {}
	~Boat() override;

	bool hasChest() const { return _chest != nullptr; }
	void setPaddleState(bool left, bool right);
	// The item use of a boat (BoatItem.use): one on the water or block the player looks at, if there is room
	static bool place(Level& level, Player& player, ItemStack& stack);

	void   tick() override;
	bool   interact(Player& player, int hand) override;
	bool   isControlledByRider() override { return hasRider(); }
	double riderY() const override { return _position.y + _height / 3.0 - 0.6; }
	void   writeEntityData(Buffer& buf) const override;
	void   save(Buffer& buf) const override;
	void   load(Buffer& buf) override;

  protected:
	ItemStack dropItem() const override;
	void	  destroy(bool drop) override;

  private:
	bool							 _paddleLeft = false, _paddleRight = false;
	std::shared_ptr<SimpleContainer> _chest;

	// The top of the water its bottom is in, if any
	std::optional<double> waterSurface() const;
	void				  floatBoat();
};

// AbstractMinecart (the old minecart behavior): rolls along rails (slopes speed it up, curves turn it, 0.4 blocks a tick at
// most), off them it rolls and slows down. Powered rails push it (and brake it when off), activator rails throw its rider
// out and prime TNT. Its rider pushes it with the forward key. Its kinds: a seat (minecart), a chest (27 slots), a
// furnace (fuel pushes it), TNT (primed, it explodes), a hopper (it picks up the items it passes). Pushing other
// minecarts and mobs, and the new experimental movement, aren't ported
class Minecart : public VehicleEntity {
  public:
	enum class Kind { Rideable, Chest, Furnace, Tnt, Hopper, Other };
	static constexpr double RAIL_HEIGHT = 0.0625;

	Minecart(Level& level, int typeId, const Vec3& position);
	Minecart(Level& level, int typeId) : Minecart(level, typeId, Vec3{}) {}
	~Minecart() override;

	Kind kind() const { return _kind; }
	int	 fuel() const { return _fuel; }
	int	 fuse() const { return _fuse; }
	bool isPrimed() const { return _fuse > -1; }
	// MinecartTNT.primeFuse: it explodes in 4 seconds
	void prime();
	// MinecartItem.useOn: a minecart of the item's kind on the rail clicked
	static bool place(Level& level, Player& player, ItemStack& stack, const BlockPos& rail);

	void   tick() override;
	bool   interact(Player& player, int hand) override;
	bool   hurtServer(const Combat::DamageSource& source, float amount) override;
	double riderY() const override { return _position.y + 0.1875 - 0.6; }
	void   writeEntityData(Buffer& buf) const override { writeVehicleData(buf); }
	void   save(Buffer& buf) const override;
	void   load(Buffer& buf) override;

  protected:
	ItemStack dropItem() const override;
	void	  destroy(bool drop) override;

  private:
	Kind							 _kind;
	std::shared_ptr<SimpleContainer> _container; // Chest and hopper minecarts
	int								 _fuel = 0, _fuse = -1;
	Vec3							 _push;
	bool							 _hopperEnabled = true;

	double maxSpeed() const;
	void   moveAlongTrack(const BlockPos& pos, int state);
	void   comeOffTrack();
	void   applyNaturalSlowdown();
	void   activate(const BlockPos& pos, bool powered);
	void   explode(double speedSqr);
	void   suckInItems();
};

#endif
