#include "world/blocks/Containers.hpp"

#include "player.hpp"
#include "world/Level.hpp"
#include "world/PlaceContext.hpp"
#include "world/Shapes.hpp"
#include "world/entity/ItemEntity.hpp"
#include "world/inventory/Menu.hpp"
#include "world/item/Components.hpp"
#include "network/buffer.hpp"
#include "network/TextComponent.hpp"

#include <algorithm>
#include <cmath>

int redstoneSignalFromContainer(Container& container, const GameData& gameData) {
	float fullness = 0.0F;
	for (int i = 0; i < container.size(); i++) {
		const ItemStack& stack = container.item(i);
		if (!stack.isEmpty()) fullness += static_cast<float>(stack.count) / container.maxStackSizeFor(stack, gameData);
	}
	fullness /= container.size();
	// Mth.lerpDiscrete(fullness, 0, 15)
	return static_cast<int>(std::floor(fullness * 14)) + (fullness > 0.0F ? 1 : 0);
}

void ContainerBlock::affectNeighborsAfterRemoval(Level& level, const BlockPos& pos, int state, bool) const {
	level.updateNeighbourForOutputSignal(pos, blockOf(state));
}

int ContainerBlock::getAnalogOutputSignal(Level& level, const BlockPos& pos, int, Direction) const {
	auto* container = dynamic_cast<Container*>(level.getBlockEntity(pos));
	return container ? redstoneSignalFromContainer(*container, level.gameData()) : 0;
}

// ===== Chests =====

ChestBlock::ChestBlock(std::shared_ptr<const BlockContext> context, std::shared_ptr<const RedstoneIds> ids, Kind kind)
	: ContainerBlock(std::move(context), std::move(ids)), _kind(kind) {
	_type		  = blocks().property("type");
	_single		  = blocks().value("single");
	_left		  = blocks().value("left");
	_right		  = blocks().value("right");
	_copperChests = _context->tag("minecraft:copper_chests");
}

// chestCanConnectTo: the same block (any copper chest for a copper chest)
bool ChestBlock::canConnectTo(int self, int other) const {
	if (_kind == Kind::Copper) return _context->inTag(_copperChests, other) && blocks().has(other, _type);
	return blockOf(other) == blockOf(self);
}

Direction ChestBlock::connectedDirection(int state) const {
	Direction facing = _context->direction(state, _context->facing);
	return blocks().get(state, _type) == _left ? clockWise(facing) : counterClockWise(facing);
}

bool ChestBlock::candidatePartnerFacing(Level& level, const BlockPos& pos, Direction side, int self, Direction& facing) const {
	int other = level.getBlockState(pos.relative(side));
	if (!canConnectTo(self, other) || blocks().get(other, _type) != _single) return false;
	facing = _context->direction(other, _context->facing);
	return true;
}

int ChestBlock::getStateForPlacement(Level& level, const PlaceContext& context) const {
	int		  self	 = blocks().defaultState(context.block);
	int		  type	 = _single;
	Direction facing = Directions::opposite(context.horizontalDirection());
	Direction face	 = context.clickedFace;
	bool	  horizontalFace = face != Direction::Up && face != Direction::Down;
	if (horizontalFace && context.secondaryUse) {
		// Sneaking against a chest's side: joins that one
		Direction partner;
		if (candidatePartnerFacing(level, context.clickedPos, Directions::opposite(face), self, partner) && Shapes::axisOf(partner) != Shapes::axisOf(face)) {
			facing = partner;
			type   = counterClockWise(partner) == Directions::opposite(face) ? _right : _left;
		}
	}
	if (type == _single && !context.secondaryUse) {
		// getChestType: a lone chest facing the same way beside it
		Direction partner;
		if (candidatePartnerFacing(level, context.clickedPos, clockWise(facing), self, partner) && partner == facing) {
			type = _left;
		} else if (candidatePartnerFacing(level, context.clickedPos, counterClockWise(facing), self, partner) && partner == facing) {
			type = _right;
		}
	}
	int state = blocks().with(blocks().with(self, _context->facing, _context->directionValue(facing)), _type, type);
	state	  = blocks().withBool(state, _context->waterlogged, level.getFluidState(context.clickedPos).type == level.fluids().water());
	if (_kind == Kind::Copper && type != _single) {
		// getLeastOxidizedChestOfConnectedBlocks: both halves become the least oxidized one (waxed ones aren't there yet)
		int other = level.getBlockState(context.clickedPos.relative(connectedDirection(state)));
		if (canConnectTo(state, other)) {
			auto age = [&](int s) {
				std::string name = _context->data.getStaticName("minecraft:block", blockOf(s));
				return name.find("oxidized") != std::string::npos ? 3 : name.find("weathered") != std::string::npos ? 2 : name.find("exposed") != std::string::npos ? 1 : 0;
			};
			if (age(other) < age(state)) state = blocks().withPropertiesOf(blockOf(other), state);
		}
	}
	return state;
}

int ChestBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos&, int neighborState) const {
	level.scheduleWaterlogged(pos, state);
	int result = state;
	if (canConnectTo(state, neighborState) && direction != Direction::Up && direction != Direction::Down) {
		int neighborType = blocks().get(neighborState, _type);
		if (blocks().get(state, _type) == _single && neighborType != _single &&
			_context->direction(state, _context->facing) == _context->direction(neighborState, _context->facing) &&
			connectedDirection(neighborState) == Directions::opposite(direction)) {
			result = blocks().with(state, _type, neighborType == _left ? _right : _left);
		}
	} else if (blocks().get(state, _type) != _single && connectedDirection(state) == direction) {
		result = blocks().with(state, _type, _single);
	}
	// CopperChestBlock: a half follows the block of the other one (its oxidation)
	if (_kind == Kind::Copper && canConnectTo(result, neighborState) && blocks().get(result, _type) != _single && connectedDirection(result) == direction) {
		return blocks().withPropertiesOf(blockOf(neighborState), result);
	}
	return result;
}

bool ChestBlock::keepsBlockEntityOf(int oldState) const { return _kind == Kind::Copper && _context->inTag(_copperChests, oldState); }

std::shared_ptr<Container> ChestBlock::container(Level& level, const BlockPos& pos, bool ignoreBlocked, std::vector<uint8_t>* title, bool* isDouble) const {
	std::shared_ptr<BlockEntity> entity = level.getSharedBlockEntity(pos);
	auto*						 chest	= dynamic_cast<ChestBlockEntity*>(entity.get());
	if (!chest || (!ignoreBlocked && isChestBlocked(level, pos))) return nullptr;
	bool						 first = false;
	std::shared_ptr<BlockEntity> other = chestPartner(level, pos, ignoreBlocked, first);
	if (!other) {
		if (title) *title = chest->customName;
		if (isDouble) *isDouble = false;
		return std::shared_ptr<Container>(entity, chest);
	}
	std::shared_ptr<BlockEntity> firstHalf = first ? entity : other, secondHalf = first ? other : entity;
	if (title) {
		// The first half's custom name, else the second's, else "Large Chest"
		auto* a = dynamic_cast<ChestBlockEntity*>(firstHalf.get());
		auto* b = dynamic_cast<ChestBlockEntity*>(secondHalf.get());
		*title	= !a->customName.empty() ? a->customName : b->customName;
	}
	if (isDouble) *isDouble = true;
	return std::make_shared<CompoundContainer>(firstHalf, secondHalf);
}

bool ChestBlock::useWithoutItem(Level& level, const BlockPos& pos, int, Player& player) const {
	std::vector<uint8_t>	   title;
	bool					   isDouble = false;
	std::shared_ptr<Container> chest	= container(level, pos, false, &title, &isDouble);
	if (chest) {
		Menus::openContainer(player, level, chest, isDouble ? "minecraft:generic_9x6" : "minecraft:generic_9x3", title,
							 isDouble ? "container.chestDouble" : "container.chest");
	}
	return true;
}

void ChestBlock::tick(Level& level, const BlockPos& pos, int) const {
	if (auto* chest = level.getBlockEntity<ChestBlockEntity>(pos)) chest->recheckOpen();
}

int ChestBlock::getAnalogOutputSignal(Level& level, const BlockPos& pos, int, Direction) const {
	std::shared_ptr<Container> chest = container(level, pos, false, nullptr, nullptr);
	return chest ? redstoneSignalFromContainer(*chest, level.gameData()) : 0;
}

int ChestBlock::getSignal(Level& level, const BlockPos& pos, int, Direction) const {
	if (_kind != Kind::Trapped) return 0;
	auto* chest = level.getBlockEntity<ChestBlockEntity>(pos);
	return chest ? std::clamp(chest->openCount(), 0, 15) : 0;
}

int ChestBlock::getDirectSignal(Level& level, const BlockPos& pos, int state, Direction direction) const {
	return direction == Direction::Up ? getSignal(level, pos, state, direction) : 0;
}

// ===== Barrel, shulker box, ender chest =====

int BarrelBlock::getStateForPlacement(Level&, const PlaceContext& context) const {
	return blocks().with(blocks().defaultState(context.block), _context->facing, _context->directionValue(Directions::opposite(context.nearestLookingDirection())));
}

bool BarrelBlock::useWithoutItem(Level& level, const BlockPos& pos, int, Player& player) const {
	std::shared_ptr<BlockEntity> entity = level.getSharedBlockEntity(pos);
	if (auto* barrel = dynamic_cast<BarrelBlockEntity*>(entity.get())) {
		Menus::openContainer(player, level, std::shared_ptr<Container>(entity, barrel), "minecraft:generic_9x3", barrel->customName, barrel->defaultName());
	}
	return true;
}

void BarrelBlock::tick(Level& level, const BlockPos& pos, int) const {
	if (auto* barrel = level.getBlockEntity<BarrelBlockEntity>(pos)) barrel->recheckOpen();
}

int ShulkerBoxBlock::getStateForPlacement(Level&, const PlaceContext& context) const {
	return blocks().with(blocks().defaultState(context.block), _context->facing, _context->directionValue(context.clickedFace));
}

bool ShulkerBoxBlock::useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const {
	std::shared_ptr<BlockEntity> entity = level.getSharedBlockEntity(pos);
	auto*						 box	= dynamic_cast<ShulkerBoxBlockEntity*>(entity.get());
	if (!box) return true;
	if (box->animation() == ShulkerBoxBlockEntity::Animation::Closed) {
		// canOpen: room for the lid (half a block in its facing)
		Direction facing = _context->direction(state, _context->facing);
		AABB	  lid{pos.x + 0.0, pos.y + 0.0, pos.z + 0.0, pos.x + 1.0, pos.y + 1.0, pos.z + 1.0};
		double	  offsets[3] = {0.5 * Directions::OFFSETS[static_cast<int>(facing)][0], 0.5 * Directions::OFFSETS[static_cast<int>(facing)][1],
								0.5 * Directions::OFFSETS[static_cast<int>(facing)][2]};
		// The half-block slice just past the box's face
		if (offsets[0] > 0) lid = {pos.x + 1.0, lid.minY, lid.minZ, pos.x + 1.5, lid.maxY, lid.maxZ};
		if (offsets[0] < 0) lid = {pos.x - 0.5, lid.minY, lid.minZ, pos.x + 0.0, lid.maxY, lid.maxZ};
		if (offsets[1] > 0) lid = {lid.minX, pos.y + 1.0, lid.minZ, lid.maxX, pos.y + 1.5, lid.maxZ};
		if (offsets[1] < 0) lid = {lid.minX, pos.y - 0.5, lid.minZ, lid.maxX, pos.y + 0.0, lid.maxZ};
		if (offsets[2] > 0) lid = {lid.minX, lid.minY, pos.z + 1.0, lid.maxX, lid.maxY, pos.z + 1.5};
		if (offsets[2] < 0) lid = {lid.minX, lid.minY, pos.z - 0.5, lid.maxX, lid.maxY, pos.z + 0.0};
		if (level.hasBlockCollision(lid.deflate(1.0E-6))) return true;
	}
	Menus::openContainer(player, level, std::shared_ptr<Container>(entity, box), "minecraft:shulker_box", box->customName, box->defaultName(),
						 Slot::Kind::NoShulkerBox);
	return true;
}

// playerWillDestroy: in creative (no drops), a box with items still drops, with them
void ShulkerBoxBlock::playerWillDestroy(Level& level, const BlockPos& pos, int state, Player& player) const {
	auto* box = level.getBlockEntity<ShulkerBoxBlockEntity>(pos);
	if (!box || player.getGameMode() != GameMode::Creative || box->isEmpty()) return;
	int		  item = level.gameData().getStaticId("minecraft:item", level.gameData().getStaticName("minecraft:block", blockOf(state)));
	ItemStack stack(item, 1);
	ContainerItems::collect(*box, stack, level.gameData());
	auto entity = ItemEntity::create(level, {pos.x + 0.5, pos.y + 0.5, pos.z + 0.5}, std::move(stack));
	entity->setPickupDelay(10);
	level.entities().add(std::move(entity));
}

int EnderChestBlock::getStateForPlacement(Level& level, const PlaceContext& context) const {
	int state = blocks().with(blocks().defaultState(context.block), _context->facing, _context->directionValue(Directions::opposite(context.horizontalDirection())));
	return blocks().withBool(state, _context->waterlogged, level.getFluidState(context.clickedPos).type == level.fluids().water());
}

bool EnderChestBlock::useWithoutItem(Level& level, const BlockPos& pos, int, Player& player) const {
	std::shared_ptr<BlockEntity> entity = level.getSharedBlockEntity(pos);
	if (!dynamic_cast<EnderChestBlockEntity*>(entity.get()) || level.isRedstoneConductor(level.getBlockState(pos.above()))) return true;
	Menus::openContainer(player, level, std::make_shared<EnderChestContainer>(player, entity), "minecraft:generic_9x3", {}, "container.enderchest");
	return true;
}

void EnderChestBlock::tick(Level& level, const BlockPos& pos, int) const {
	if (auto* chest = level.getBlockEntity<EnderChestBlockEntity>(pos)) chest->recheckOpen();
}

// ===== Crafting table =====

bool CraftingTableBlock::useWithoutItem(Level& level, const BlockPos& pos, int, Player& player) const {
	Menus::openCrafting(player, level, pos);
	return true;
}

// ===== Hopper =====

int HopperBlock::getStateForPlacement(Level&, const PlaceContext& context) const {
	Direction into	 = Directions::opposite(context.clickedFace);
	Direction facing = into == Direction::Up || into == Direction::Down ? Direction::Down : into;
	int		  state	 = blocks().with(blocks().defaultState(context.block), _context->facing, _context->directionValue(facing));
	return blocks().withBool(state, blocks().property("enabled"), true);
}

void HopperBlock::checkPoweredState(Level& level, const BlockPos& pos, int state) const {
	bool enabled = !level.hasNeighborSignal(pos);
	int	 id		 = blocks().property("enabled");
	if (enabled != blocks().getBool(state, id)) level.setBlock(pos, blocks().withBool(state, id, enabled), Level::UPDATE_CLIENTS);
}

void HopperBlock::onPlace(Level& level, const BlockPos& pos, int state, int oldState, bool) const {
	if (blockOf(oldState) != blockOf(state)) checkPoweredState(level, pos, state);
}

void HopperBlock::neighborChanged(Level& level, const BlockPos& pos, int state, int, bool) const { checkPoweredState(level, pos, state); }

bool HopperBlock::useWithoutItem(Level& level, const BlockPos& pos, int, Player& player) const {
	std::shared_ptr<BlockEntity> entity = level.getSharedBlockEntity(pos);
	if (auto* hopper = dynamic_cast<HopperBlockEntity*>(entity.get())) {
		Menus::openContainer(player, level, std::shared_ptr<Container>(entity, hopper), "minecraft:hopper", hopper->customName, hopper->defaultName());
	}
	return true;
}

void HopperBlock::entityInside(Level& level, const BlockPos& pos, int, Entity* entity) const {
	auto* item	 = dynamic_cast<ItemEntity*>(entity);
	auto* hopper = level.getBlockEntity<HopperBlockEntity>(pos);
	if (item && hopper && !item->isRemoved()) hopper->entityInside(level, *item);
}

// ===== Components =====

namespace ContainerItems {
	void collect(const BlockEntity& entity, ItemStack& stack, const GameData& gameData) {
		auto* container = dynamic_cast<const ContainerBlockEntity*>(&entity);
		if (!container) return;
		bool anyItem = std::any_of(container->items().begin(), container->items().end(), [](const ItemStack& s) { return !s.isEmpty(); });
		if (anyItem) Components::set(stack, gameData, "minecraft:container", Components::encodeContainer(container->items()));
		if (!container->customName.empty()) Components::set(stack, gameData, "minecraft:custom_name", container->customName);
	}

	void apply(BlockEntity& entity, const ItemStack& stack, const GameData& gameData) {
		auto* container = dynamic_cast<ContainerBlockEntity*>(&entity);
		if (!container || stack.components.empty()) return;
		if (std::optional<std::vector<uint8_t>> name = Components::get(stack, gameData, "minecraft:custom_name")) container->customName = *name;
		std::optional<std::vector<uint8_t>> contents = Components::get(stack, gameData, "minecraft:container");
		if (!contents) return;
		std::optional<std::vector<ItemStack>> items = Components::decodeContainer(*contents, gameData);
		if (!items) return;
		// ItemContainerContents.copyInto
		for (size_t i = 0; i < container->items().size(); i++) container->items()[i] = i < items->size() ? (*items)[i] : ItemStack();
		container->setChanged();
	}
} // namespace ContainerItems
