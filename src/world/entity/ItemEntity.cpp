#include "world/entity/ItemEntity.hpp"

#include "data/GameData.hpp"
#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/entity/EntityManager.hpp"

#include <algorithm>

namespace {
	constexpr int ITEM_DATA_INDEX		= 8; // ItemEntity.DATA_ITEM
	constexpr int ITEM_STACK_SERIALIZER = 7; // EntityDataSerializers.ITEM_STACK
} // namespace

ItemEntity::ItemEntity(Level& level, const Vec3& position, ItemStack stack, const Vec3& delta)
	: Entity(level, level.gameData().getStaticId("minecraft:entity_type", "minecraft:item"), 0.25f, 0.25f), _stack(std::move(stack)) {
	_random.nextFloat(); // The entity's own random draws its yaw
	_position = position;
	_delta	  = delta;
}

std::unique_ptr<ItemEntity> ItemEntity::create(Level& level, const Vec3& position, ItemStack stack) {
	JavaRandom& random = level.random();
	double		dx	   = random.nextDouble() * 0.2 - 0.1;
	double		dz	   = random.nextDouble() * 0.2 - 0.1;
	return std::make_unique<ItemEntity>(level, position, std::move(stack), Vec3{dx, 0.2, dz});
}

void ItemEntity::setItem(ItemStack stack) {
	_stack			= std::move(stack);
	entityDataDirty = true;
}

int ItemEntity::maxStackSize(const ItemStack& stack) const {
	const GameData::ItemProperties* item = _level.gameData().getItemProperties(stack.item);
	return item ? item->maxStackSize : 64;
}

bool ItemEntity::fireImmune() const {
	const GameData::ItemProperties* item = _level.gameData().getItemProperties(_stack.item);
	return item && item->fireResistant;
}

void ItemEntity::tick() {
	if (_stack.isEmpty()) {
		discard();
		return;
	}
	Entity::tick();
	if (_pickupDelay > 0 && _pickupDelay != INFINITE_PICKUP_DELAY) _pickupDelay--;
	Vec3 previous	   = _position;
	Vec3 previousDelta = _delta;
	if (isInWater() && _waterHeight > 0.1f) {
		setFluidMovement(0.99f);
	} else if (isInLava() && _lavaHeight > 0.1f) {
		setFluidMovement(0.95f);
	} else {
		applyGravity(0.04);
	}

	AABB box   = boundingBox();
	_noPhysics = !noCollision(box.deflate(1.0E-7));
	if (_noPhysics) moveTowardsClosestSpace(_position.x, (box.minY + box.maxY) / 2.0, _position.z);

	if (!_onGround || _delta.horizontalDistanceSqr() > 1.0E-5f || (_tickCount + _id) % 4 == 0) {
		move(_delta);
		applyEffectsFromBlocks();
		float friction = 0.98f;
		if (_onGround) friction = _level.gameData().getBlockProperties(_level.getBlockState(blockPosBelowAffectingMovement())).friction * 0.98f;
		_delta = _delta.multiply(friction, 0.98, friction);
		if (_onGround && _delta.y < 0.0) _delta.y *= -0.5;
	}

	bool moved = Mth::floor(previous.x) != Mth::floor(_position.x) || Mth::floor(previous.y) != Mth::floor(_position.y) ||
				 Mth::floor(previous.z) != Mth::floor(_position.z);
	if (_tickCount % (moved ? 2 : 40) == 0 && isMergable()) mergeWithNeighbours();
	if (_age != INFINITE_LIFETIME) _age++;
	hasImpulse = hasImpulse | updateInWaterStateAndDoFluidPushing();
	if ((_delta - previousDelta).lengthSqr() > 0.01) hasImpulse = true;
	if (_age >= LIFETIME) discard();
}

void ItemEntity::setFluidMovement(double factor) { _delta = {_delta.x * factor, _delta.y + (_delta.y < 0.06f ? 5.0E-4f : 0.0f), _delta.z * factor}; }

void ItemEntity::applyEffectsFromBlocks() {
	if (_removed) return;
	if (isInLava() && !fireImmune()) {
		hurt(4.0f); // Entity.lavaHurt
		if (_removed) return;
	}
	// Blocks the item is inside: each effect once per tick
	const GameData& data = _level.gameData();
	AABB			box	 = boundingBox().deflate(1.0E-5);
	float			fire = 0.0f;
	bool			cactus = false;
	for (int x = Mth::floor(box.minX); x <= Mth::floor(box.maxX); x++) {
		for (int y = Mth::floor(box.minY); y <= Mth::floor(box.maxY); y++) {
			for (int z = Mth::floor(box.minZ); z <= Mth::floor(box.maxZ); z++) {
				int block = data.getBlocks().blockOf(_level.getBlockState({x, y, z}));
				if (block == _level.fireBlock()) fire = std::max(fire, 1.0f);
				if (block == _level.soulFireBlock()) fire = std::max(fire, 2.0f);
				if (block == _level.cactusBlock()) cactus = true;
			}
		}
	}
	if (fire > 0.0f && !fireImmune()) hurt(fire);
	if (cactus && !_removed) hurt(1.0f);
}

void ItemEntity::hurt(float amount) {
	_health = static_cast<int>(_health - amount);
	if (_health <= 0) discard();
}

bool ItemEntity::isMergable() const {
	return !_removed && _pickupDelay != INFINITE_PICKUP_DELAY && _age != INFINITE_LIFETIME && _age < LIFETIME && _stack.count < maxStackSize(_stack);
}

void ItemEntity::mergeWithNeighbours() {
	if (!isMergable()) return;
	for (ItemEntity* other : _level.entities().itemsIn(boundingBox().inflate(0.5, 0.0, 0.5))) {
		if (other == this || !other->isMergable()) continue;
		tryToMerge(*other);
		if (_removed) break;
	}
}

// Both stacks are the same item and fit together: the bigger one takes the other
void ItemEntity::tryToMerge(ItemEntity& other) {
	ItemStack& mine	  = _stack;
	ItemStack& theirs = other._stack;
	if (theirs.count + mine.count > maxStackSize(theirs) || !mine.sameItemSameComponents(theirs)) return;
	ItemEntity& into = theirs.count < mine.count ? *this : other;
	ItemEntity& from = theirs.count < mine.count ? other : *this;
	int			moved = std::min(std::min(maxStackSize(into._stack), 64) - into._stack.count, from._stack.count);
	into.setItem(into._stack.copyWithCount(into._stack.count + moved));
	from._stack.shrink(moved); // Changed in place, like vanilla: not sent
	into._pickupDelay = std::max(into._pickupDelay, from._pickupDelay);
	into._age		  = std::min(into._age, from._age);
	if (from._stack.isEmpty()) from.discard();
}

int ItemEntity::playerTouch(Player& player) {
	int count = _stack.count;
	if (_pickupDelay != 0) return 0;
	bool creative = player.getGameMode() == GameMode::Creative;
	if (!player.inventory().add(_stack, player.getSelectedSlot(), creative, _level.gameData())) return 0;
	if (_stack.isEmpty()) discard();
	return count;
}

void ItemEntity::writeEntityData(Buffer& buf) const {
	buf.writeUByte(ITEM_DATA_INDEX);
	buf.writeVarInt(ITEM_STACK_SERIALIZER);
	_stack.write(buf);
}

void ItemEntity::save(Buffer& buf) const {
	buf.writeUUID(_uuid);
	for (double v : {_position.x, _position.y, _position.z, _delta.x, _delta.y, _delta.z}) buf.writeDouble(v);
	buf.writeFloat(_yRot);
	buf.writeVarInt(_stack.item);
	buf.writeVarInt(_stack.count);
	buf.writeVarInt(static_cast<int32_t>(_stack.components.size()));
	buf.writeBytes(_stack.components);
	buf.writeInt(_age);
	buf.writeInt(_pickupDelay);
	buf.writeInt(_health);
}

void ItemEntity::load(Buffer& buf) {
	_uuid		= buf.readUUID();
	_position.x = buf.readDouble();
	_position.y = buf.readDouble();
	_position.z = buf.readDouble();
	_delta.x	= buf.readDouble();
	_delta.y	= buf.readDouble();
	_delta.z	= buf.readDouble();
	_yRot		= buf.readFloat();
	_stack.item	 = buf.readVarInt();
	_stack.count = buf.readVarInt();
	_stack.components = buf.readBytes(static_cast<size_t>(buf.readVarInt()));
	_age		 = buf.readInt();
	_pickupDelay = buf.readInt();
	_health		 = buf.readInt();
	_oldPosition = _position;
}
