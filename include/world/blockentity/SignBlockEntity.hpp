#ifndef SIGN_BLOCK_ENTITY_HPP
#define SIGN_BLOCK_ENTITY_HPP

#include "lib/UUID.hpp"
#include "world/blockentity/BlockEntity.hpp"

#include <array>
#include <optional>
#include <string>

// SignText: one side of a sign, 4 lines of plain text, a dye color and glowing ink
struct SignText {
	static constexpr int LINES = 4;
	std::array<std::string, LINES> messages;
	std::string					   color	   = "black"; // DyeColor's name
	bool						   glowingText = false;

	bool hasMessage() const;
};

// SignBlockEntity (signs and hanging signs): both sides' text, the wax that locks them, and the player allowed to
// edit them (the one whose text editor is open)
class SignBlockEntity : public BlockEntity {
  public:
	// SignBlockEntity.MAX_TEXT_LINE_WIDTH is a client limit: here, the length the client may send at most
	static constexpr size_t MAX_LINE_LENGTH = 384;

	SignBlockEntity(const std::string& type, const BlockPos& pos) : BlockEntity(type, pos) {}

	SignText&		text(bool front) { return front ? _front : _back; }
	const SignText& text(bool front) const { return front ? _front : _back; }
	bool			isWaxed() const { return _waxed; }
	void			setWaxed(bool waxed);
	// playerWhoMayEdit: set when a player's editor opens, cleared when it sends its text
	const std::optional<UUID>& playerWhoMayEdit() const { return _editor; }
	void					   setAllowedPlayerEditor(std::optional<UUID> editor) { _editor = editor; }
	// isTooFarAwayToEdit / playerIsTooFarAwayToEdit: within 4 blocks of reach (the default block interaction range)
	// plus a margin
	bool canEdit(const UUID& player) const { return !_waxed && _editor && *_editor == player; }
	// updateSignText: the lines a player sent (formatting codes removed), then the clients see them
	void updateText(bool front, const std::array<std::string, SignText::LINES>& lines);
	// The text changed: saved, sent again to the players around
	void setChanged();

	void save(BlockEntityWriter& out) const override;
	void load(BlockEntityReader& in) override;
	void writeUpdateTag(std::vector<uint8_t>& out) const override;
	bool hasUpdatePacket() const override { return true; }

  private:
	SignText			_front, _back;
	bool				_waxed = false;
	std::optional<UUID> _editor;
};

#endif
