#include "world/blockentity/BlockEntity.hpp"

#include "data/GameData.hpp"
#include "lib/JavaRandom.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/blockentity/ContainerEntities.hpp"
#include "world/entity/ItemEntity.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

// ----- Saving -----

void BlockEntityWriter::varint(uint32_t value) {
	while (value >= 0x80) {
		out.push_back(static_cast<uint8_t>(value | 0x80));
		value >>= 7;
	}
	out.push_back(static_cast<uint8_t>(value));
}

void BlockEntityWriter::f32(float value) {
	uint32_t bits;
	std::memcpy(&bits, &value, 4);
	for (int i = 0; i < 4; i++) out.push_back(static_cast<uint8_t>(bits >> (8 * i)));
}

void BlockEntityWriter::string(const std::string& value) {
	varint(static_cast<uint32_t>(value.size()));
	out.insert(out.end(), value.begin(), value.end());
}

void BlockEntityWriter::bytes(const std::vector<uint8_t>& value) {
	varint(static_cast<uint32_t>(value.size()));
	out.insert(out.end(), value.begin(), value.end());
}

void BlockEntityWriter::item(const ItemStack& stack) {
	if (stack.isEmpty()) {
		string("");
		return;
	}
	string(gameData.getStaticName("minecraft:item", stack.item));
	varint(static_cast<uint32_t>(stack.count));
	bytes(stack.components);
}

void BlockEntityReader::need(size_t count) const {
	if (_pos + count > _size) throw std::runtime_error("block entity data too short");
}

uint8_t BlockEntityReader::u8() {
	need(1);
	return _data[_pos++];
}

uint32_t BlockEntityReader::varint() {
	uint32_t value = 0;
	for (int shift = 0; shift < 35; shift += 7) {
		uint8_t byte = u8();
		value |= static_cast<uint32_t>(byte & 0x7F) << shift;
		if (!(byte & 0x80)) return value;
	}
	throw std::runtime_error("block entity varint too long");
}

float BlockEntityReader::f32() {
	need(4);
	uint32_t bits = 0;
	for (int i = 0; i < 4; i++) bits |= static_cast<uint32_t>(_data[_pos++]) << (8 * i);
	float value;
	std::memcpy(&value, &bits, 4);
	return value;
}

std::string BlockEntityReader::string() {
	uint32_t length = varint();
	need(length);
	std::string value(reinterpret_cast<const char*>(_data + _pos), length);
	_pos += length;
	return value;
}

std::vector<uint8_t> BlockEntityReader::bytes() {
	uint32_t length = varint();
	need(length);
	std::vector<uint8_t> value(_data + _pos, _data + _pos + length);
	_pos += length;
	return value;
}

ItemStack BlockEntityReader::item() {
	std::string name = string();
	if (name.empty()) return {};
	ItemStack stack(gameData.getStaticId("minecraft:item", name), static_cast<int>(varint()));
	stack.components = bytes();
	// An item this version doesn't have is lost
	return stack.item > 0 ? stack : ItemStack();
}

// ----- Block entities -----

std::unique_ptr<BlockEntity> BlockEntity::create(const std::string& type, const BlockPos& pos) {
	if (type == "minecraft:comparator") return std::make_unique<ComparatorBlockEntity>(pos);
	if (type == "minecraft:piston") return std::make_unique<PistonMovingBlockEntity>(pos);
	if (std::unique_ptr<BlockEntity> container = createContainerBlockEntity(type, pos)) return container;
	if (type.empty()) return nullptr;
	return std::make_unique<GenericBlockEntity>(type, pos);
}

void BlockEntity::markChanged() {
	if (_level) _level->blockEntityChanged(_pos);
}

void PistonMovingBlockEntity::tick(Level& level) { level.tickMovingPiston(*this); }
void PistonMovingBlockEntity::finalTick(Level& level) { level.finalTickMovingPiston(*this); }

void PistonMovingBlockEntity::save(BlockEntityWriter& out) const {
	out.string(out.gameData.getBlockStateName(movedState));
	out.u8(static_cast<uint8_t>(direction));
	out.f32(progressO); // Like vanilla: the progress of the last tick
	out.u8(extending);
	out.u8(sourcePiston);
}

void PistonMovingBlockEntity::load(BlockEntityReader& in) {
	int state	 = in.gameData.getBlockStateFromName(in.string());
	movedState	 = state >= 0 ? state : in.gameData.getDefaultBlockState("minecraft:air");
	direction	 = static_cast<Direction>(std::min<uint8_t>(in.u8(), 5));
	progress	 = in.f32();
	progressO	 = progress;
	extending	 = in.u8() != 0;
	sourcePiston = in.u8() != 0;
}

// ----- Containers -----

void Container::setItem(int slot, ItemStack stack) {
	item(slot) = std::move(stack);
	setChanged();
}

bool Container::isEmpty() {
	for (int i = 0; i < size(); i++) {
		if (!item(i).isEmpty()) return false;
	}
	return true;
}

// ContainerHelper.removeItem
ItemStack Container::removeItem(int slot, int count) {
	if (slot < 0 || slot >= size() || item(slot).isEmpty() || count <= 0) return {};
	ItemStack& stack = item(slot);
	int		   taken = std::min(count, stack.count);
	ItemStack  split = stack.copyWithCount(taken);
	stack.shrink(taken);
	if (stack.count <= 0) stack = ItemStack();
	setChanged();
	return split;
}

std::vector<int> Container::slotsForFace(Direction) {
	std::vector<int> slots(static_cast<size_t>(size()));
	for (int i = 0; i < size(); i++) slots[i] = i;
	return slots;
}

int Container::maxStackSizeFor(const ItemStack& stack, const GameData& gameData) const {
	const GameData::ItemProperties* item = gameData.getItemProperties(stack.item);
	return std::min(maxStackSize(), item ? item->maxStackSize : 64);
}

ItemStack Container::removeItemNoUpdate(int slot) {
	ItemStack stack = std::move(item(slot));
	item(slot)		= ItemStack();
	return stack;
}

