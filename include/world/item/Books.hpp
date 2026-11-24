#ifndef BOOKS_HPP
#define BOOKS_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class Level;
class Player;

// Book and quill editing and signing (ServerGamePacketListenerImpl.handleEditBook, updateBookContents, signBook), and
// reading a written book (WrittenBookItem.use -> ServerPlayer.openItemGui)
namespace Books {
	constexpr int	 MAX_PAGES		 = 100;	 // WritableBookContent.MAX_PAGES
	constexpr size_t PAGE_LENGTH	 = 1024; // WritableBookContent.PAGE_EDIT_LENGTH
	constexpr size_t TITLE_LENGTH	 = 32;	 // WrittenBookContent.TITLE_MAX_LENGTH

	// ServerboundEditBookPacket: the pages of the book and quill in that slot (hotbar 0-8, or 40 for the offhand), and
	// with a title, the book signed (a written book by the player). Ignored for anything else in the slot
	void handleEdit(Level& level, Player& player, int slot, const std::vector<std::string>& pages, const std::optional<std::string>& title);
	// A written book in that hand: the client opens it (Open Book). Whether it was one
	bool openWritten(Level& level, Player& player, int hand);

	// Reads the pages of minecraft:writable_book_content, empty if it can't be read
	std::vector<std::string> writablePages(const std::vector<uint8_t>& component);
} // namespace Books

#endif
