#include "world/entity/Vehicles.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Clip.hpp"
#include "world/Combat.hpp"
#include "world/Fluids.hpp"
#include "world/Level.hpp"
#include "world/entity/ItemEntity.hpp"
#include "world/inventory/Menu.hpp"

#include <cmath>
#include <stdexcept>

namespace {
	constexpr int SAVE_VERSION		 = 1;
	constexpr int SERIALIZER_INT	 = 1, SERIALIZER_FLOAT = 3, SERIALIZER_BOOLEAN = 8;

	void writeStack(Buffer& buf, const ItemStack& stack) {
		buf.writeVarInt(stack.isEmpty() ? 0 : stack.item);
		buf.writeVarInt(stack.isEmpty() ? 0 : stack.count);
		buf.writeVarInt(static_cast<int32_t>(stack.components.size()));
		buf.writeBytes(stack.components);
	}
	ItemStack readStack(Buffer& buf) {
		ItemStack stack;
		stack.item		 = buf.readVarInt();
		stack.count		 = buf.readVarInt();
		stack.components = buf.readBytes(static_cast<size_t>(buf.readVarInt()));
		return stack.isEmpty() ? ItemStack{} : stack;
	}
} // namespace

// ===================== VehicleEntity =====================

ItemStack VehicleEntity::itemNamed(const std::string& name) const {
	int id = _level.gameData().getStaticId("minecraft:item", name);
	return id >= 0 ? ItemStack(id, 1) : ItemStack{};
}

bool VehicleEntity::hurtServer(const Combat::DamageSource& source, float amount) {
	if (isRemoved()) return true;
	// VehicleEntity.hurtServer: a creative player breaks it at once (no drop)
	Player* player	  = source.attackerPlayer();
	bool	instantKill = player && player->isCreative();
	_hurtDir		  = -_hurtDir;
	_hurtTime		  = 10;
	_damage += amount * 10.0F;
	entityDataDirty = true;
	if (instantKill || _damage > 40.0F) destroy(!instantKill);
	return true;
}

void VehicleEntity::destroy(bool drop) {
	if (drop) {
		ItemStack item = dropItem();
		if (!item.isEmpty()) _level.entities().add(ItemEntity::create(_level, _position, std::move(item)));
	}
	if (hasRider()) ejectRider();
	discard();
}

void VehicleEntity::tickHurt() {
	if (_hurtTime > 0) {
		_hurtTime--;
		entityDataDirty = true;
	}
	if (_damage > 0.0F) {
		_damage = std::max(0.0F, _damage - 1.0F);
		entityDataDirty = true;
	}
}

void VehicleEntity::writeVehicleData(Buffer& buf) const {
	if (_hurtTime != 0) {
		buf.writeUByte(DATA_HURT);
		buf.writeVarInt(SERIALIZER_INT);
		buf.writeVarInt(_hurtTime);
	}
	if (_hurtDir != 1) {
		buf.writeUByte(DATA_HURTDIR);
		buf.writeVarInt(SERIALIZER_INT);
		buf.writeVarInt(_hurtDir);
	}
	if (_damage != 0.0F) {
		buf.writeUByte(DATA_DAMAGE);
		buf.writeVarInt(SERIALIZER_FLOAT);
		buf.writeFloat(_damage);
	}
}

void VehicleEntity::saveVehicle(Buffer& buf) const {
	buf.writeUByte(SAVE_VERSION);
	saveBase(buf);
}

void VehicleEntity::loadVehicle(Buffer& buf) {
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown vehicle data version");
	loadBase(buf);
}

// ===================== Boat =====================

Boat::Boat(Level& level, int typeId, const Vec3& position, float yRot) : VehicleEntity(level, typeId, 1.375F, 0.5625F) {
	snapTo(position, yRot, 0.0F);
	// The raft and boat types come with their own size
	if (const GameData::EntityTypeInfo* type = level.gameData().getEntityType(typeId)) _width = type->width, _height = type->height;
	if (level.gameData().getEntityType(typeId) && level.gameData().getEntityType(typeId)->is("AbstractChestBoat")) {
		_chest = std::make_shared<SimpleContainer>(27);
	}
}

Boat::~Boat() = default;

ItemStack Boat::dropItem() const { return itemNamed(_level.gameData().getStaticName("minecraft:entity_type", _typeId)); }

void Boat::destroy(bool drop) {
	// ChestBoat.destroy: its items too
	if (_chest && drop) {
		for (int slot = 0; slot < _chest->size(); slot++) {
			ItemStack& stack = _chest->item(slot);
			if (!stack.isEmpty()) _level.entities().add(ItemEntity::create(_level, _position, stack));
			stack = ItemStack{};
		}
	}
	VehicleEntity::destroy(drop);
}

void Boat::setPaddleState(bool left, bool right) {
	if (left == _paddleLeft && right == _paddleRight) return;
	_paddleLeft		= left;
	_paddleRight	= right;
	entityDataDirty = true;
}

std::optional<double> Boat::waterSurface() const {
	// The highest water level among the blocks under its bottom (Boat.getWaterLevelAbove)
	AABB				  box = boundingBox();
	std::optional<double> surface;
	for (int x = Mth::floor(box.minX); x <= Mth::floor(box.maxX); x++) {
		for (int z = Mth::floor(box.minZ); z <= Mth::floor(box.maxZ); z++) {
			for (int y = Mth::floor(box.minY) - 1; y <= Mth::floor(box.minY) + 1; y++) {
				FluidState fluid = _level.getFluidState({x, y, z});
				if (!_level.fluids().isWater(fluid.type)) continue;
				// FluidState.getHeight: a source or falling water fills 8/9 of its block
				double top = y + (fluid.falling ? 1.0 : fluid.amount / 9.0);
				if (!surface || top > *surface) surface = top;
			}
		}
	}
	return surface;
}

void Boat::floatBoat() {
	// Boat.floatBoat: gravity, buoyancy toward the surface, and the friction of where it is
	double				  gravity  = -0.04;
	double				  friction = 0.05;
	std::optional<double> water	   = waterSurface();
	double				  dy	   = 0.0;
	if (water && *water > _position.y - 0.001) {
		dy		 = (*water - _position.y) / _height;
		friction = 0.9;
	} else if (_onGround) {
		friction = 0.6 * 0.9; // A boat drags on the ground (Block.getFriction 0.6)
	} else {
		friction = 0.9;
	}
	_delta = {_delta.x * friction, _delta.y + gravity, _delta.z * friction};
	if (dy > 0.0) _delta.y = (_delta.y + dy * 0.06153846) * 0.75;
}

void Boat::tick() {
	tickHurt();
	baseTick();
	if (isRemoved()) return;
	if (hasRider()) {
		// Its rider's client moves it; the rider follows
		positionRider();
		return;
	}
	setPaddleState(false, false);
	floatBoat();
	move(_delta);
}

bool Boat::interact(Player& player, int hand) {
	if (hand != 0) return false;
	// Sneaking at a chest boat opens its chest; else it sits in the boat (if it is free)
	if (player.isShiftKeyDown()) {
		if (!_chest) return false;
		std::string type = _level.gameData().getStaticName("minecraft:entity_type", _typeId);
		Menus::openContainer(player, _level, _chest, "minecraft:generic_9x3", {}, "entity.minecraft." + type.substr(type.find(':') + 1));
		return true;
	}
	if (hasRider() || player.vehicleId() != -1) return false;
	return startRiding(player);
}

bool Boat::place(Level& level, Player& player, ItemStack& stack) {
	// BoatItem.use: along the player's look, 5 blocks at most, onto water or a block
	const float toRadians = static_cast<float>(M_PI / 180.0);
	float		yaw = player.getYaw() * toRadians, pitch = player.getPitch() * toRadians;
	Vec3		look{-Mth::sin(yaw) * Mth::cos(pitch), -Mth::sin(pitch), Mth::cos(yaw) * Mth::cos(pitch)};
	Vec3		eyes{player.getX(), player.eyeY(), player.getZ()};
	Clip::HitResult hit = Clip::clip(level, eyes, eyes + look.scale(5.0), Clip::BlockMode::Outline, Clip::FluidMode::Any);
	if (!hit.hit) return false;
	int type = level.gameData().getStaticId("minecraft:entity_type", level.gameData().getStaticName("minecraft:item", stack.item));
	if (type < 0) return false;
	auto boat = std::make_unique<Boat>(level, type, hit.location, player.getYaw());
	// Room for it: no block and no other entity in its way
	AABB box = boat->boundingBox().deflate(0.1);
	if (level.hasBlockCollision(box)) return false;
	for (Actor* actor : level.actorsIn(box.inflate(0.1))) {
		if (actor != &player && !actor->isSpectator()) return false;
	}
	level.entities().add(std::move(boat));
	if (!player.isCreative()) stack.shrink(1);
	return true;
}

void Boat::writeEntityData(Buffer& buf) const {
	writeVehicleData(buf);
	if (_paddleLeft) {
		buf.writeUByte(DATA_PADDLE_LEFT);
		buf.writeVarInt(SERIALIZER_BOOLEAN);
		buf.writeBool(true);
	}
	if (_paddleRight) {
		buf.writeUByte(DATA_PADDLE_RIGHT);
		buf.writeVarInt(SERIALIZER_BOOLEAN);
		buf.writeBool(true);
	}
}

void Boat::save(Buffer& buf) const {
	saveVehicle(buf);
	buf.writeBool(_chest != nullptr);
	if (_chest) {
		for (int slot = 0; slot < _chest->size(); slot++) writeStack(buf, _chest->item(slot));
	}
}

void Boat::load(Buffer& buf) {
	loadVehicle(buf);
	if (buf.readBool()) {
		if (!_chest) _chest = std::make_shared<SimpleContainer>(27);
		for (int slot = 0; slot < _chest->size(); slot++) _chest->item(slot) = readStack(buf);
	}
}
