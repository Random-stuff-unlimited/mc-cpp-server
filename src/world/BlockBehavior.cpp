#include "world/BlockBehavior.hpp"

#include "world/Explosion.hpp"
#include "world/Level.hpp"
#include "world/blockentity/BlockEntity.hpp"

void BlockBehavior::tick(Level&, const BlockPos&, int) const {}
bool BlockBehavior::isRandomlyTicking(int) const { return false; }
void BlockBehavior::randomTick(Level&, const BlockPos&, int) const {}
void BlockBehavior::neighborChanged(Level&, const BlockPos&, int, int, bool) const {}
// What most blocks without a ported behavior do: stairs, slabs, fences... only let their water flow (their shape
// connections aren't ported yet)
int BlockBehavior::updateShape(Level& level, const BlockPos& pos, int state, Direction, const BlockPos&, int) const {
	level.scheduleWaterlogged(pos, state);
	return state;
}
void BlockBehavior::updateIndirectNeighbourShapes(Level&, const BlockPos&, int, int, int) const {}
void BlockBehavior::onPlace(Level&, const BlockPos&, int, int, bool) const {}
void BlockBehavior::affectNeighborsAfterRemoval(Level&, const BlockPos&, int, bool) const {}
bool BlockBehavior::triggerEvent(Level&, const BlockPos&, int, int, int) const { return false; }
bool BlockBehavior::isSignalSource(int) const { return false; }
int	 BlockBehavior::getSignal(Level&, const BlockPos&, int, Direction) const { return 0; }
int	 BlockBehavior::getDirectSignal(Level&, const BlockPos&, int, Direction) const { return 0; }
bool BlockBehavior::hasAnalogOutputSignal(int) const { return false; }
int	 BlockBehavior::getAnalogOutputSignal(Level&, const BlockPos&, int, Direction) const { return 0; }
int	 BlockBehavior::getStateForPlacement(Level&, const PlaceContext&) const { return GENERIC_PLACEMENT; }
bool BlockBehavior::canSurvive(Level&, const BlockPos&, int) const { return true; }
void BlockBehavior::setPlacedBy(Level&, const BlockPos&, int) const {}
bool BlockBehavior::useWithoutItem(Level&, const BlockPos&, int, Player&) const { return false; }
UseResult BlockBehavior::useItemOn(Level&, const BlockPos&, int, Player&, int, const BlockHit&) const { return UseResult::TryWithEmptyHand; }
void BlockBehavior::entityInside(Level&, const BlockPos&, int, Actor*) const {}
void BlockBehavior::playerWillDestroy(Level&, const BlockPos&, int, Player&) const {}
bool BlockBehavior::keepsBlockEntityOf(int) const { return false; }
std::unique_ptr<BlockEntity> BlockBehavior::newBlockEntity(const BlockPos&, int) const { return nullptr; }

BlockBehaviors::BlockBehaviors(size_t blockCount) : _byBlock(blockCount, &_default), _randomTickers(blockCount, nullptr), _placers(blockCount, nullptr) {}

void BlockBehaviors::setPlacement(int block, std::unique_ptr<BlockBehavior> behavior) {
	_placers.at(block) = behavior.get();
	_owned.push_back(std::move(behavior));
}

void BlockBehaviors::setRandomTick(int block, std::unique_ptr<BlockBehavior> behavior) {
	_randomTickers.at(block) = behavior.get();
	_owned.push_back(std::move(behavior));
}

void BlockBehaviors::set(int block, std::unique_ptr<BlockBehavior> behavior) {
	_byBlock.at(block) = behavior.get();
	_owned.push_back(std::move(behavior));
}

void BlockBehavior::onExplosionHit(Level& level, const BlockPos& pos, int state, Explosions::Explosion& explosion,
								   const std::function<void(ItemStack, const BlockPos&)>& drop) const {
	if (level.blocks().isAir(state) || explosion.interaction == Explosions::BlockInteraction::TriggerBlock) return;
	if (dropFromExplosion()) {
		static const ItemStack NO_TOOL;
		LootTables::Context context{level, pos, state, &NO_TOOL, explosion.source != nullptr,
									explosion.interaction == Explosions::BlockInteraction::DestroyWithDecay ? explosion.radius : 0.0F};
		context.blockEntity = level.getBlockEntity(pos);
		for (ItemStack& stack : level.loot().blockDrops(context)) drop(std::move(stack), pos);
	}
	level.setBlock(pos, level.gameData().getDefaultBlockState("minecraft:air"), Level::UPDATE_ALL);
	wasExploded(level, pos, explosion);
}
