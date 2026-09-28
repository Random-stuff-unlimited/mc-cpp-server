#ifndef ITEM_STACK_HPP
#define ITEM_STACK_HPP

#include <cstdint>
#include <vector>

class Buffer;

// A stack of items (vanilla's ItemStack). Components (enchantments, custom name...) aren't understood yet: they are
// kept as the client sent them (the "component patch" of the Slot format, after the item id) and sent back as is
struct ItemStack {
	int					 item  = 0; // minecraft:item id, 0 = air
	int					 count = 0;
	std::vector<uint8_t> components; // Component patch as on the network; empty = the item's default components

	ItemStack() = default;
	ItemStack(int itemId, int itemCount) : item(itemId), count(itemCount) {}

	bool	  isEmpty() const { return item == 0 || count <= 0; }
	ItemStack copyWithCount(int newCount) const {
		ItemStack copy = *this;
		copy.count	   = newCount;
		return copy;
	}
	void shrink(int amount) { count -= amount; }
	void grow(int amount) { count += amount; }
	// ItemStack.isSameItemSameComponents
	bool sameItemSameComponents(const ItemStack& other) const;

	// Slot format: VarInt count, then if not empty: VarInt item id and the component patch
	void			 write(Buffer& buf) const;
	// Reads a Slot that ends the packet (the component patch can't be measured without understanding it)
	static ItemStack readLast(Buffer& buf);
};

#endif
