#include "world/entity/mobs/SnowGolem.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/Throwables.hpp"
#include "world/entity/ai/Goals.hpp"
#include "world/item/ItemDamage.hpp"

#include <cmath>

namespace {
	constexpr int SAVE_VERSION = 1;

	// SnowLayerBlock.canSurvive
	bool snowCanSurvive(Level& level, const BlockPos& pos) {
		const GameData& data  = level.gameData();
		int				state = level.getBlockState(pos.below());
		int				block = level.blocks().blockOf(state);
		if (data.isInTag("minecraft:block", "minecraft:snow_layer_cannot_survive_on", block)) return false;
		if (data.isInTag("minecraft:block", "minecraft:snow_layer_can_survive_on", block)) return true;
		// Block.isFaceFull(collision shape, UP), or a full stack of snow layers
		for (const GameData::Box& box : data.getCollisionShape(state)) {
			if (box.minX <= 0.0 && box.maxX >= 1.0 && box.minZ <= 0.0 && box.maxZ >= 1.0 && box.maxY >= 1.0) return true;
		}
		return false;
	}
} // namespace

void SnowGolem::registerGoals() {
	_goalSelector.addGoal(1, std::make_unique<RangedAttackGoal>(*this, 1.25, 20, 10.0F));
	_goalSelector.addGoal(2, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 1.0, 1.0000001E-5F));
	_goalSelector.addGoal(3, std::make_unique<LookAtPlayerGoal>(*this, "Player", 6.0F));
	_goalSelector.addGoal(4, std::make_unique<RandomLookAroundGoal>(*this));
	// Any mob that is an Enemy
	_targetSelector.addGoal(1, std::make_unique<NearestAttackableTargetGoal>(*this, "Mob", 10, true, false, [](Actor& target, Level& level) {
		const GameData::EntityTypeInfo* type = level.gameData().getEntityType(target.typeId());
		return type && type->is("Enemy");
	}));
}

void SnowGolem::setPumpkin(bool pumpkin) {
	if (pumpkin == _pumpkin) return;
	_pumpkin = pumpkin;
	markData(DATA_PUMPKIN);
}

void SnowGolem::aiStep() {
	Mob::aiStep();
	if (isRemoved()) return;
	if (_level.gameData().isInTag("minecraft:worldgen/biome", "minecraft:snow_golem_melts", _level.getBiomeId(blockPosition()))) {
		hurtServer({"minecraft:on_fire", nullptr}, 1.0F);
	}
	// Its trail of snow (mobGriefing: always on here)
	int snow = _level.gameData().getDefaultBlockState("minecraft:snow");
	for (int i = 0; i < 4; i++) {
		BlockPos pos{Mth::floor(_position.x + (i % 2 * 2 - 1) * 0.25F), Mth::floor(_position.y), Mth::floor(_position.z + (i / 2 % 2 * 2 - 1) * 0.25F)};
		if (_level.blocks().isAir(_level.getBlockState(pos)) && snowCanSurvive(_level, pos)) _level.setBlock(pos, snow, Level::UPDATE_ALL);
	}
}

void SnowGolem::performRangedAttack(Actor& target, float) {
	double dx	= target.position().x - _position.x;
	double y	= target.eyeY() - 1.1F;
	double dz	= target.position().z - _position.z;
	double lift = std::sqrt(dx * dx + dz * dz) * 0.2F;
	auto   ball = std::make_unique<Snowball>(_level, this);
	ball->shoot(dx, y + lift - ball->position().y, dz, 1.6F, 12.0F);
	_level.entities().add(std::move(ball));
	playSound("minecraft:entity.snow_golem.shoot", 1.0F, 0.4F / (_random.nextFloat() * 0.4F + 0.8F));
}

void SnowGolem::shear() {
	_level.playSoundAt(nullptr, _position.x, _position.y, _position.z, "minecraft:entity.snow_golem.shear", Level::SoundSource::Players, 1.0F, 1.0F);
	setPumpkin(false);
	for (ItemStack& stack : rollLootTable("minecraft:shearing/snow_golem")) spawnAtLocation(std::move(stack), eyeHeight());
}

bool SnowGolem::mobInteract(Player& player, int hand) {
	const ItemStack& stack = player.getStackInHand(hand);
	if (!stack.isEmpty() && stack.item == _level.gameData().getStaticId("minecraft:item", "minecraft:shears") && readyForShearing()) {
		shear();
		ItemDamage::hurtAndBreak(_level, player.inventory().getMutable(player.handSlot(hand)), 1, &player, player.handSlot(hand));
		return true;
	}
	return false;
}

void SnowGolem::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Mob::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_PUMPKIN, onlyNonDefault, _pumpkin)) writeByteData(buf, DATA_PUMPKIN, _pumpkin ? 16 : 0);
}

void SnowGolem::save(Buffer& buf) const {
	Mob::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeBool(_pumpkin);
}

void SnowGolem::load(Buffer& buf) {
	Mob::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown snow golem data version");
	_pumpkin   = buf.readBool();
	_dirtyData = 0;
}
