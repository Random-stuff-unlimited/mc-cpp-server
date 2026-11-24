#include "world/item/Books.hpp"

#include "PacketIds.hpp"
#include "data/GameData.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/item/Components.hpp"

namespace {
	std::string clip(const std::string& text, size_t length) {
		if (text.size() <= length) return text;
		// Cut at a character boundary (UTF-8 continuation bytes are 10xxxxxx)
		size_t end = length;
		while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80) end--;
		return text.substr(0, end);
	}

	// Filterable<String> without a filtered version: the raw text, then "no filtered text"
	void writeFilterable(Buffer& out, const std::string& text) {
		out.writeString(text);
		out.writeBool(false);
	}

	// A text component as network NBT: a string tag is a plain text component
	void writeTextComponent(Buffer& out, const std::string& text) {
		out.writeUByte(8);
		out.writeUShort(static_cast<uint16_t>(text.size()));
		std::vector<uint8_t> bytes(text.begin(), text.end());
		out.writeBytes(bytes);
	}

	bool isBlank(const std::string& text) { return text.find_first_not_of(" \t") == std::string::npos; }
} // namespace

namespace Books {
	void handleEdit(Level& level, Player& player, int slot, const std::vector<std::string>& pages, const std::optional<std::string>& title) {
		// Inventory.isHotbarSlot, or Inventory.SLOT_OFFHAND
		int inventorySlot;
		if (slot >= 0 && slot < 9) {
			inventorySlot = PlayerInventory::HOTBAR + slot;
		} else if (slot == 40) {
			inventorySlot = PlayerInventory::OFFHAND;
		} else {
			return;
		}
		const GameData& data  = level.gameData();
		ItemStack		stack = player.inventory().get(inventorySlot);
		if (stack.isEmpty() || data.getStaticName("minecraft:item", stack.item) != "minecraft:writable_book") return;
		if (pages.size() > static_cast<size_t>(MAX_PAGES)) return;
		std::vector<std::string> kept;
		for (const std::string& page : pages) kept.push_back(clip(page, PAGE_LENGTH));

		if (!title) {
			// updateBookContents: the pages, as typed
			Buffer content;
			content.writeVarInt(static_cast<int32_t>(kept.size()));
			for (const std::string& page : kept) writeFilterable(content, page);
			Components::set(stack, data, "minecraft:writable_book_content", content.getData());
			player.inventory().set(inventorySlot, stack);
			return;
		}
		// signBook: a written book, original (generation 0), by the player; its pages become text
		std::string name = clip(*title, TITLE_LENGTH);
		if (isBlank(name)) return;
		ItemStack written(data.getStaticId("minecraft:item", "minecraft:written_book"), stack.count);
		Buffer	  content;
		writeFilterable(content, name);
		content.writeString(player.getPlayerName());
		content.writeVarInt(0); // Generation: original
		content.writeVarInt(static_cast<int32_t>(kept.size()));
		for (const std::string& page : kept) {
			writeTextComponent(content, page);
			content.writeBool(false);
		}
		content.writeBool(true); // Resolved: nothing left to compute in its text
		Components::set(written, data, "minecraft:written_book_content", content.getData());
		player.inventory().set(inventorySlot, written);
	}

	bool openWritten(Level& level, Player& player, int hand) {
		const ItemStack& stack = player.getStackInHand(hand);
		if (stack.isEmpty() || level.gameData().getStaticName("minecraft:item", stack.item) != "minecraft:written_book") return false;
		Buffer open;
		open.writeVarInt(hand);
		Packet::send(player.shared_from_this(), PacketId::Play::Clientbound::OPEN_BOOK, open, level.server());
		return true;
	}

	std::vector<std::string> writablePages(const std::vector<uint8_t>& component) {
		std::vector<std::string> pages;
		try {
			Buffer in(component);
			for (int i = in.readVarInt(); i > 0; i--) {
				pages.push_back(in.readString(PAGE_LENGTH * 4));
				if (in.readBool()) in.readString(PAGE_LENGTH * 4);
			}
		} catch (const std::exception&) {
			return {};
		}
		return pages;
	}
} // namespace Books
