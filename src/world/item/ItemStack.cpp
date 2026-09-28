#include "world/item/ItemStack.hpp"

#include "network/buffer.hpp"

namespace {
	// No component change: 0 added, 0 removed
	const std::vector<uint8_t> NO_COMPONENTS = {0, 0};

	const std::vector<uint8_t>& patchOf(const ItemStack& stack) { return stack.components.empty() ? NO_COMPONENTS : stack.components; }
} // namespace

bool ItemStack::sameItemSameComponents(const ItemStack& other) const {
	if (isEmpty() || other.isEmpty()) return isEmpty() && other.isEmpty();
	return item == other.item && patchOf(*this) == patchOf(other);
}

void ItemStack::write(Buffer& buf) const {
	if (isEmpty()) {
		buf.writeVarInt(0);
		return;
	}
	buf.writeVarInt(count);
	buf.writeVarInt(item);
	buf.writeBytes(patchOf(*this));
}

ItemStack ItemStack::readLast(Buffer& buf) {
	ItemStack stack;
	stack.count = buf.readVarInt();
	if (stack.count <= 0) return ItemStack();
	stack.item		 = buf.readVarInt();
	stack.components = buf.readBytes(buf.remaining());
	if (stack.components == NO_COMPONENTS) stack.components.clear();
	return stack;
}
