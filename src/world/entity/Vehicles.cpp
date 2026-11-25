#include "world/entity/Vehicles.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Clip.hpp"
#include "world/Combat.hpp"
#include "world/Fluids.hpp"
#include "world/Explosion.hpp"
#include "world/Level.hpp"
#include "world/blocks/Rails.hpp"
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
	if (!canAddPassenger() || player.vehicleId() != -1) return false;
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

// ===================== Minecart =====================

namespace {
	// The ends of a rail shape (AbstractMinecart.EXITS): two steps (dx, dy, dz), dy for the end that goes down a slope
	struct Exit {
		int x, y, z;
	};
	std::pair<Exit, Exit> exitsOf(const std::string& shape) {
		if (shape == "east_west") return {{-1, 0, 0}, {1, 0, 0}};
		if (shape == "ascending_east") return {{-1, -1, 0}, {1, 0, 0}};
		if (shape == "ascending_west") return {{-1, 0, 0}, {1, -1, 0}};
		if (shape == "ascending_north") return {{0, 0, -1}, {0, -1, 1}};
		if (shape == "ascending_south") return {{0, -1, -1}, {0, 0, 1}};
		if (shape == "south_east") return {{0, 0, 1}, {1, 0, 0}};
		if (shape == "south_west") return {{0, 0, 1}, {-1, 0, 0}};
		if (shape == "north_west") return {{0, 0, -1}, {-1, 0, 0}};
		if (shape == "north_east") return {{0, 0, -1}, {1, 0, 0}};
		return {{0, 0, -1}, {0, 0, 1}};
	}
	bool isRailAt(Level& level, const BlockPos& pos) {
		return level.gameData().isInTag("minecraft:block", "minecraft:rails", level.blocks().blockOf(level.getBlockState(pos)));
	}
	std::string blockNameAt(Level& level, const BlockPos& pos) {
		return level.gameData().getStaticName("minecraft:block", level.blocks().blockOf(level.getBlockState(pos)));
	}
} // namespace

Minecart::Minecart(Level& level, int typeId, const Vec3& position) : VehicleEntity(level, typeId, 0.98F, 0.7F) {
	snapTo(position, 0.0F, 0.0F);
	std::string name = level.gameData().getStaticName("minecraft:entity_type", typeId);
	_kind			 = name == "minecraft:minecart"			  ? Kind::Rideable
					 : name == "minecraft:chest_minecart"	  ? Kind::Chest
					 : name == "minecraft:furnace_minecart"	  ? Kind::Furnace
					 : name == "minecraft:tnt_minecart"		  ? Kind::Tnt
					 : name == "minecraft:hopper_minecart"	  ? Kind::Hopper
															  : Kind::Other;
	if (_kind == Kind::Chest) _container = std::make_shared<SimpleContainer>(27);
	if (_kind == Kind::Hopper) _container = std::make_shared<SimpleContainer>(5);
}

Minecart::~Minecart() = default;

double Minecart::maxSpeed() const {
	// AbstractMinecart.getMaxSpeed: 8 blocks a second (4 for the furnace one), half in water
	double speed = _kind == Kind::Furnace ? 0.2 : 0.4;
	return isInWater() ? speed / 2.0 : speed;
}

ItemStack Minecart::dropItem() const {
	switch (_kind) {
		case Kind::Chest: return itemNamed("minecraft:chest_minecart");
		case Kind::Furnace: return itemNamed("minecraft:furnace_minecart");
		case Kind::Tnt: return itemNamed("minecraft:tnt_minecart");
		case Kind::Hopper: return itemNamed("minecraft:hopper_minecart");
		default: return itemNamed("minecraft:minecart");
	}
}

void Minecart::destroy(bool drop) {
	if (_container && drop) {
		for (int slot = 0; slot < _container->size(); slot++) {
			ItemStack& stack = _container->item(slot);
			if (!stack.isEmpty()) _level.entities().add(ItemEntity::create(_level, _position, stack));
			stack = ItemStack{};
		}
	}
	VehicleEntity::destroy(drop);
}

bool Minecart::place(Level& level, Player& player, ItemStack& stack, const BlockPos& rail) {
	if (!isRailAt(level, rail)) return false;
	int type = level.gameData().getStaticId("minecraft:entity_type", level.gameData().getStaticName("minecraft:item", stack.item));
	if (type < 0) return false;
	std::string shape = level.gameData().getProperty(level.getBlockState(rail), "shape");
	double		y	  = rail.y + RAIL_HEIGHT + (shape.rfind("ascending", 0) == 0 ? 0.5 : 0.0);
	auto		cart  = std::make_unique<Minecart>(level, type, Vec3{rail.x + 0.5, y, rail.z + 0.5});
	cart->setYRot(player.getYaw());
	level.entities().add(std::move(cart));
	if (!player.isCreative()) stack.shrink(1);
	return true;
}

void Minecart::prime() {
	if (_fuse > -1) return;
	_fuse = 80;
	_level.playSoundAt(nullptr, _position.x, _position.y, _position.z, "minecraft:entity.tnt.primed", Level::SoundSource::Blocks, 1.0F, 1.0F);
}

void Minecart::explode(double speedSqr) {
	// MinecartTNT.explode: 4, more the faster it went
	double speed = std::min(std::sqrt(speedSqr), 5.0);
	float  power = static_cast<float>(4.0 + _random.nextDouble() * 1.5 * speed);
	discard();
	Explosions::explode(_level, this, std::nullopt, nullptr, _position, power, false, Explosions::Interaction::Tnt);
}

bool Minecart::hurtServer(const Combat::DamageSource& source, float amount) {
	if (_kind == Kind::Tnt && !isRemoved()) {
		// MinecartTNT.hurtServer: fire primes it, an explosion sets it off
		const GameData& data = _level.gameData();
		int				type = data.getSyncedId("minecraft:damage_type", source.type);
		if (data.isInTag("minecraft:damage_type", "minecraft:is_explosion", type)) {
			explode(_delta.lengthSqr());
			return true;
		}
		if (data.isInTag("minecraft:damage_type", "minecraft:is_fire", type)) {
			prime();
			return true;
		}
	}
	return VehicleEntity::hurtServer(source, amount);
}

bool Minecart::interact(Player& player, int hand) {
	if (hand != 0) return false;
	switch (_kind) {
		case Kind::Rideable:
			if (player.isShiftKeyDown() || hasRider() || player.vehicleId() != -1) return false;
			return startRiding(player);
		case Kind::Chest:
			Menus::openContainer(player, _level, _container, "minecraft:generic_9x3", {}, "entity.minecraft.chest_minecart");
			return true;
		case Kind::Hopper:
			Menus::openContainer(player, _level, _container, "minecraft:hopper", {}, "entity.minecraft.hopper_minecart");
			return true;
		case Kind::Furnace: {
			// MinecartFurnace.interact: fuel (coal, charcoal) for 3 minutes, pushed away from the player
			ItemStack& stack = player.inventory().getMutable(player.handSlot(hand));
			if (!stack.isEmpty() && _level.gameData().isInTag("minecraft:item", "minecraft:furnace_minecart_fuel", stack.item) && _fuel + 3600 <= 32000) {
				if (!player.isCreative()) stack.shrink(1);
				_fuel += 3600;
			}
			if (_fuel > 0) _push = {_position.x - player.getX(), 0.0, _position.z - player.getZ()};
			return true;
		}
		default: return false;
	}
}

void Minecart::activate(const BlockPos&, bool powered) {
	// activateMinecart: an activator rail
	switch (_kind) {
		case Kind::Rideable:
			if (powered) {
				if (hasRider()) ejectRider();
				if (_hurtTime == 0) {
					_hurtDir		= -_hurtDir;
					_hurtTime		= 10;
					_damage			= 50.0F;
					entityDataDirty = true;
				}
			}
			break;
		case Kind::Tnt:
			if (powered) prime();
			break;
		case Kind::Hopper: _hopperEnabled = !powered; break;
		default: break;
	}
}

void Minecart::applyNaturalSlowdown() {
	if (_kind == Kind::Furnace) {
		// MinecartFurnace.applyNaturalSlowdown: its push keeps it going
		double push = _push.horizontalDistanceSqr();
		if (push > 1.0E-7) {
			push   = std::sqrt(push);
			_push  = {_push.x / push, 0.0, _push.z / push};
			_delta = _delta.multiply(0.8, 0.0, 0.8) + _push.scale(0.2);
		} else {
			_delta = _delta.multiply(0.98, 0.0, 0.98);
		}
		return;
	}
	double friction = hasRider() ? 0.997 : 0.96;
	_delta			= _delta.multiply(friction, 0.0, friction);
}

void Minecart::moveAlongTrack(const BlockPos& pos, int state) {
	// OldMinecartBehavior.moveAlongTrack
	_fallDistance			= 0.0;
	std::string block		= blockNameAt(_level, pos);
	bool		poweredRail = block == "minecraft:powered_rail";
	bool		powered		= poweredRail && _level.gameData().getProperty(state, "powered") == "true";
	bool		brake		= poweredRail && !powered;
	std::string shape		= _level.gameData().getProperty(state, "shape");
	double		slope		= isInWater() ? 0.0078125 * 0.2 : 0.0078125;
	if (shape == "ascending_east") _delta.x -= slope;
	if (shape == "ascending_west") _delta.x += slope;
	if (shape == "ascending_north") _delta.z += slope;
	if (shape == "ascending_south") _delta.z -= slope;
	auto [exit0, exit1] = exitsOf(shape);
	double dx = exit1.x - exit0.x, dz = exit1.z - exit0.z;
	double length = std::sqrt(dx * dx + dz * dz);
	if (_delta.x * dx + _delta.z * dz < 0.0) dx = -dx, dz = -dz;
	double speed = std::min(2.0, std::sqrt(_delta.horizontalDistanceSqr()));
	_delta		 = {speed * dx / length, _delta.y, speed * dz / length};
	// Its rider's push (the forward key, toward where it looks), when it barely moves
	if (Player* player = rider(); player && player->isForwardKeyDown()) {
		const float toRadians = static_cast<float>(M_PI / 180.0);
		Vec3		intent{-Mth::sin(player->getYaw() * toRadians), 0.0, Mth::cos(player->getYaw() * toRadians)};
		if (_delta.horizontalDistanceSqr() < 0.01) {
			_delta = _delta + intent.scale(0.1);
			brake  = false;
		}
	}
	if (brake) {
		double h = std::sqrt(_delta.horizontalDistanceSqr());
		_delta	 = h < 0.03 ? Vec3{0.0, _delta.y, 0.0} : _delta.multiply(0.5, 0.0, 0.5);
	}
	// Onto the rail's line
	double x0 = pos.x + 0.5 + exit0.x * 0.5, z0 = pos.z + 0.5 + exit0.z * 0.5;
	double x1 = pos.x + 0.5 + exit1.x * 0.5, z1 = pos.z + 0.5 + exit1.z * 0.5;
	double lx = x1 - x0, lz = z1 - z0, t;
	if (lx == 0.0) {
		t = _position.z - pos.z;
	} else if (lz == 0.0) {
		t = _position.x - pos.x;
	} else {
		t = ((_position.x - x0) * lx + (_position.z - z0) * lz) * 2.0;
	}
	_position.x = x0 + lx * t;
	_position.z = z0 + lz * t;
	double scale = hasRider() ? 0.75 : 1.0;
	double max	 = maxSpeed();
	Vec3   step{std::clamp(scale * _delta.x, -max, max), 0.0, std::clamp(scale * _delta.z, -max, max)};
	_position.x += step.x;
	_position.z += step.z;
	applyNaturalSlowdown();
	// Into the next block: its movement turns to follow
	int bx = Mth::floor(_position.x), bz = Mth::floor(_position.z);
	if (bx != pos.x || bz != pos.z) {
		double h = std::sqrt(_delta.horizontalDistanceSqr());
		_delta	 = {h * (bx - pos.x), _delta.y, h * (bz - pos.z)};
	}
	// A powered rail pushes it on, or off a wall to start
	if (powered) {
		double h = std::sqrt(_delta.horizontalDistanceSqr());
		if (h > 0.01) {
			_delta = _delta + Vec3{_delta.x / h * 0.06, 0.0, _delta.z / h * 0.06};
		} else if (shape == "east_west") {
			if (!_level.gameData().getCollisionShape(_level.getBlockState(pos.relative(Direction::West))).empty()) _delta.x = 0.02;
			else if (!_level.gameData().getCollisionShape(_level.getBlockState(pos.relative(Direction::East))).empty()) _delta.x = -0.02;
		} else if (shape == "north_south") {
			if (!_level.gameData().getCollisionShape(_level.getBlockState(pos.relative(Direction::North))).empty()) _delta.z = 0.02;
			else if (!_level.gameData().getCollisionShape(_level.getBlockState(pos.relative(Direction::South))).empty()) _delta.z = -0.02;
		}
	}
	// Its height on the rail it is now on (up a slope as it goes)
	BlockPos now{Mth::floor(_position.x), pos.y, Mth::floor(_position.z)};
	if (!isRailAt(_level, now) && isRailAt(_level, now.below())) now = now.below();
	if (!isRailAt(_level, now) && isRailAt(_level, now.above())) now = now.above();
	double y = now.y + RAIL_HEIGHT;
	if (isRailAt(_level, now)) {
		std::string s  = _level.gameData().getProperty(_level.getBlockState(now), "shape");
		double		fx = _position.x - now.x, fz = _position.z - now.z;
		if (s == "ascending_east") y += fx;
		if (s == "ascending_west") y += 1.0 - fx;
		if (s == "ascending_north") y += 1.0 - fz;
		if (s == "ascending_south") y += fz;
	}
	_position.y = y;
	_delta.y	= 0.0;
	_onGround	= true;
	if (_delta.horizontalDistanceSqr() > 1.0E-4) _yRot = static_cast<float>(Mth::atan2(_delta.z, _delta.x) * 180.0 / M_PI);
}

void Minecart::comeOffTrack() {
	// AbstractMinecart.comeOffTrack: rolls, half speed on the ground
	double max = maxSpeed();
	_delta	   = {std::clamp(_delta.x, -max, max), _delta.y, std::clamp(_delta.z, -max, max)};
	if (_onGround) _delta = _delta.scale(0.5);
	move(_delta);
	if (!_onGround) _delta = _delta.scale(0.95);
}

void Minecart::suckInItems() {
	// MinecartHopper: the items it touches above its middle
	AABB box = boundingBox().inflate(0.25, 0.0, 0.25);
	for (Actor* actor : _level.actorsIn(box, this)) {
		auto* item = dynamic_cast<ItemEntity*>(actor);
		if (!item || item->isRemoved()) continue;
		ItemStack stack = item->item();
		for (int slot = 0; slot < _container->size() && !stack.isEmpty(); slot++) {
			ItemStack& in = _container->item(slot);
			if (in.isEmpty()) {
				in	  = stack;
				stack = ItemStack{};
			} else if (in.sameItemSameComponents(stack) && in.count < 64) {
				int moved = std::min(64 - in.count, stack.count);
				in.grow(moved);
				stack.shrink(moved);
			}
		}
		if (stack.isEmpty() || stack.count <= 0) {
			item->discard();
		} else {
			item->setItem(stack);
		}
	}
}

void Minecart::tick() {
	tickHurt();
	baseTick();
	if (isRemoved()) return;
	// MinecartTNT: its fuse
	if (_fuse > 0) {
		if (--_fuse == 0) {
			explode(_delta.lengthSqr());
			return;
		}
	}
	if (_kind == Kind::Furnace) {
		if (_fuel > 0) _fuel--;
		if (_fuel <= 0) _push = {};
	}
	// On a rail (its own block, or the one under it): along it
	BlockPos pos{Mth::floor(_position.x), Mth::floor(_position.y), Mth::floor(_position.z)};
	if (isRailAt(_level, pos.below())) pos = pos.below();
	if (isRailAt(_level, pos)) {
		int state = _level.getBlockState(pos);
		if (_kind == Kind::Furnace && _delta.horizontalDistanceSqr() > 0.001 && _push.horizontalDistanceSqr() > 1.0E-4) {
			// The push follows the track
			_push = _push.x * _delta.x + _push.z * _delta.z < 0.0 ? Vec3{} : Vec3{_delta.x, 0.0, _delta.z};
		}
		moveAlongTrack(pos, state);
		std::string block = blockNameAt(_level, pos);
		if (block == "minecraft:activator_rail") activate(pos, _level.gameData().getProperty(state, "powered") == "true");
		if (block == "minecraft:detector_rail") RailBehavior::minecartOn(_level, pos);
	} else {
		_delta.y -= 0.04;
		comeOffTrack();
	}
	if (_kind == Kind::Hopper && _hopperEnabled && _tickCount % 4 == 0) suckInItems();
	positionRider();
}

void Minecart::save(Buffer& buf) const {
	saveVehicle(buf);
	buf.writeInt(_fuel);
	buf.writeDouble(_push.x);
	buf.writeDouble(_push.z);
	buf.writeInt(_fuse);
	buf.writeBool(_container != nullptr);
	if (_container) {
		for (int slot = 0; slot < _container->size(); slot++) writeStack(buf, _container->item(slot));
	}
}

void Minecart::load(Buffer& buf) {
	loadVehicle(buf);
	_fuel	= buf.readInt();
	_push.x = buf.readDouble();
	_push.z = buf.readDouble();
	_fuse	= buf.readInt();
	if (buf.readBool() && _container) {
		for (int slot = 0; slot < _container->size(); slot++) _container->item(slot) = readStack(buf);
	}
}
