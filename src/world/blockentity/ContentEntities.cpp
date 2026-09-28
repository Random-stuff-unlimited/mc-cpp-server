#include "world/blockentity/StorageEntities.hpp"

#include "data/GameData.hpp"
#include "PacketIds.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "lib/JavaRandom.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/entity/ItemEntity.hpp"
#include "world/item/Components.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
	constexpr int LEVEL_EVENT_JUKEBOX_START = 1010;
	constexpr int LEVEL_EVENT_JUKEBOX_STOP	= 1011;

	int step(Direction direction, int axis) { return Directions::OFFSETS[static_cast<int>(direction)][axis]; }

	// Container.stillValidBlockEntity: still there, and the player within its interaction range + 4
	bool stillValidBlockEntity(BlockEntity& entity, Player& player) {
		Level* at = entity.level();
		if (!at || entity.isRemoved() || at->getBlockEntity(entity.pos()) != &entity) return false;
		const BlockPos& pos	  = entity.pos();
		double			range = blockInteractionRange(player) + 4.0;
		double			eyeY  = player.getY() + 1.62;
		double			dx	  = std::max({pos.x - player.getX(), 0.0, player.getX() - (pos.x + 1.0)});
		double			dy	  = std::max({pos.y - eyeY, 0.0, eyeY - (pos.y + 1.0)});
		double			dz	  = std::max({pos.z - player.getZ(), 0.0, player.getZ() - (pos.z + 1.0)});
		return dx * dx + dy * dy + dz * dz < range * range;
	}

	// ContainerHelper.removeItem: splits the slot
	ItemStack splitSlot(ItemStack& slot, int count) {
		if (slot.isEmpty() || count <= 0) return {};
		int		  taken = std::min(count, slot.count);
		ItemStack split = slot.copyWithCount(taken);
		slot.shrink(taken);
		if (slot.count <= 0) slot = ItemStack();
		return split;
	}

	// Container.hasAnyMatching
	template <typename Predicate> bool hasAnyMatching(Container& container, Predicate predicate) {
		for (int i = 0; i < container.size(); i++) {
			if (predicate(container.item(i))) return true;
		}
		return false;
	}

	// ----- Network NBT -----
	void u16(std::vector<uint8_t>& out, size_t value) {
		out.push_back(static_cast<uint8_t>(value >> 8));
		out.push_back(static_cast<uint8_t>(value));
	}
	void i32(std::vector<uint8_t>& out, int32_t value) {
		for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<uint8_t>(static_cast<uint32_t>(value) >> shift));
	}
	void name(std::vector<uint8_t>& out, uint8_t type, const char* tag) {
		out.push_back(type);
		u16(out, std::strlen(tag));
		out.insert(out.end(), tag, tag + std::strlen(tag));
	}
	void stringPayload(std::vector<uint8_t>& out, const std::string& value) {
		u16(out, value.size());
		out.insert(out.end(), value.begin(), value.end());
	}
	// ItemStack.CODEC: {id, count}
	void stackFields(std::vector<uint8_t>& out, const ItemStack& stack, const GameData& gameData) {
		name(out, 8, "id");
		stringPayload(out, gameData.getStaticName("minecraft:item", stack.item));
		name(out, 3, "count");
		i32(out, stack.count);
	}

	// Saved single stack (empty string for none)
	void writeStacks(BlockEntityWriter& out, const std::vector<ItemStack>& items) {
		out.varint(static_cast<uint32_t>(items.size()));
		for (const ItemStack& stack : items) out.item(stack);
	}
	void readStacks(BlockEntityReader& in, std::vector<ItemStack>& items) {
		std::fill(items.begin(), items.end(), ItemStack());
		uint32_t count = in.varint();
		for (uint32_t i = 0; i < count; i++) {
			ItemStack stack = in.item();
			if (i < items.size()) items[i] = std::move(stack);
		}
	}

	// ItemEntity at a point, with the default pickup delay (10 ticks) and its random throw
	void dropAt(Level& level, double x, double y, double z, ItemStack stack) {
		auto entity = ItemEntity::create(level, {x, y, z}, std::move(stack));
		entity->setPickupDelay(10);
		level.entities().add(std::move(entity));
	}

} // namespace


// ===== Jukebox =====

namespace {
	// JukeboxSongs.bootstrap: length in seconds and comparator output
	constexpr JukeboxSong SONGS[] = {
			{"minecraft:13", 178, 1},		  {"minecraft:cat", 185, 2},	   {"minecraft:blocks", 345, 3},	 {"minecraft:chirp", 185, 4},
			{"minecraft:far", 174, 5},		  {"minecraft:mall", 197, 6},	   {"minecraft:mellohi", 96, 7},	 {"minecraft:stal", 150, 8},
			{"minecraft:strad", 188, 9},	  {"minecraft:ward", 251, 10},	   {"minecraft:11", 71, 11},		 {"minecraft:wait", 238, 12},
			{"minecraft:pigstep", 149, 13},	  {"minecraft:otherside", 195, 14}, {"minecraft:5", 178, 15},		 {"minecraft:relic", 218, 14},
			{"minecraft:precipice", 299, 13}, {"minecraft:creator", 176, 12},  {"minecraft:creator_music_box", 73, 11}, {"minecraft:tears", 175, 10},
			{"minecraft:lava_chicken", 134, 9},
	};
} // namespace

int JukeboxSong::lengthInTicks() const { return static_cast<int>(std::ceil(lengthInSeconds * 20.0F)); }

const JukeboxSong* JukeboxSong::byName(const std::string& name) {
	for (const JukeboxSong& song : SONGS) {
		if (name == song.name) return &song;
	}
	return nullptr;
}

// The component in the stack's patch, else the item's own: the music discs play the song of their name
// (music_disc_cat: minecraft:cat)
const JukeboxSong* JukeboxSong::fromStack(const ItemStack& stack, const GameData& gameData) {
	if (stack.isEmpty()) return nullptr;
	if (std::optional<std::vector<uint8_t>> value = Components::get(stack, gameData, "minecraft:jukebox_playable")) {
		std::optional<Components::JukeboxSongRef> ref = Components::decodeJukeboxPlayable(*value);
		if (!ref) return nullptr;
		if (!ref->key.empty()) return byName(ref->key);
		for (const JukeboxSong& song : SONGS) {
			if (ref->registryId >= 0 && gameData.getSyncedId("minecraft:jukebox_song", song.name) == ref->registryId) return &song;
		}
		return nullptr;
	}
	if (Components::isRemoved(stack, gameData, "minecraft:jukebox_playable")) return nullptr;
	const std::string& item	  = gameData.getStaticName("minecraft:item", stack.item);
	const std::string  prefix = "minecraft:music_disc_";
	if (item.rfind(prefix, 0) != 0) return nullptr;
	return byName("minecraft:" + item.substr(prefix.size()));
}

bool JukeboxBlockEntity::canPlaceItem(int slot, const ItemStack& stack) const {
	return level() && JukeboxSong::fromStack(stack, level()->gameData()) && (slot != 0 || _item.isEmpty());
}

bool JukeboxBlockEntity::canTakeItem(Container& target, int, const ItemStack&) {
	return hasAnyMatching(target, [](const ItemStack& other) { return other.isEmpty(); });
}

bool JukeboxBlockEntity::stillValid(Player& player) { return stillValidBlockEntity(*this, player); }

void JukeboxBlockEntity::setItem(int slot, ItemStack stack) {
	if (slot == 0) setTheItem(std::move(stack));
}

// splitTheItem: the whole disc
ItemStack JukeboxBlockEntity::removeItem(int slot, int) {
	if (slot != 0) return {};
	ItemStack taken = _item;
	setTheItem(ItemStack());
	return taken;
}

void JukeboxBlockEntity::setTheItem(ItemStack stack) {
	_item = stack.count > 0 ? std::move(stack) : ItemStack();
	if (!level()) return;
	Level&			   at	  = *level();
	bool			   has	  = !_item.isEmpty();
	const JukeboxSong* song	  = JukeboxSong::fromStack(_item, at.gameData());
	// notifyItemChangedInJukebox: only while the jukebox is there (not while it is being replaced)
	int					 state = at.getBlockState(pos());
	const BlockRegistry& reg   = at.blocks();
	if (reg.blockOf(state) == at.gameData().getStaticId("minecraft:block", "minecraft:jukebox")) {
		at.setBlock(pos(), reg.withBool(state, reg.property("has_record"), has), Level::UPDATE_CLIENTS);
	}
	if (has && song) {
		play(*song);
	} else {
		stop();
	}
}

void JukeboxBlockEntity::play(const JukeboxSong& song) {
	_song				   = &song;
	_ticksSinceSongStarted = 0;
	Level& at			   = *level();
	at.levelEvent(nullptr, LEVEL_EVENT_JUKEBOX_START, pos(), at.gameData().getSyncedId("minecraft:jukebox_song", song.name));
	onSongChanged();
}

void JukeboxBlockEntity::stop() {
	if (!_song) return;
	_song				   = nullptr;
	_ticksSinceSongStarted = 0;
	level()->levelEvent(nullptr, LEVEL_EVENT_JUKEBOX_STOP, pos(), 0);
	onSongChanged();
}

void JukeboxBlockEntity::onSongChanged() {
	Level& at = *level();
	at.updateNeighborsAt(pos(), at.blocks().blockOf(at.getBlockState(pos())));
	markChanged();
}

// JukeboxSongPlayer.tick: stops 20 ticks after the song's end; notes every second
void JukeboxBlockEntity::tick(Level& level) {
	if (!_song) return;
	if (_song->hasFinished(_ticksSinceSongStarted)) {
		stop();
		return;
	}
	if (_ticksSinceSongStarted % 20 == 0) {
		float color = level.random().nextInt(4) / 24.0F;
		sendParticles(level, "minecraft:note", pos().x + 0.5, pos().y + 1.2F, pos().z + 0.5, 0, color, 0.0, 0.0, 1.0);
	}
	_ticksSinceSongStarted++;
}

int JukeboxBlockEntity::comparatorOutput() const {
	const JukeboxSong* song = level() ? JukeboxSong::fromStack(_item, level()->gameData()) : nullptr;
	return song ? song->comparatorOutput : 0;
}

void JukeboxBlockEntity::popOutTheItem() {
	if (!level() || _item.isEmpty()) return;
	Level&		at	   = *level();
	ItemStack	disc   = _item;
	setTheItem(ItemStack());
	JavaRandom& random = at.random();
	// Vec3.atLowerCornerWithOffset(pos, 0.5, 1.01, 0.5).offsetRandom(random, 0.7)
	double x = pos().x + 0.5 + (random.nextFloat() - 0.5F) * 0.7F;
	double y = pos().y + 1.01 + (random.nextFloat() - 0.5F) * 0.7F;
	double z = pos().z + 0.5 + (random.nextFloat() - 0.5F) * 0.7F;
	dropAt(at, x, y, z, std::move(disc));
}

void JukeboxBlockEntity::preRemoveSideEffects(Level& level) {
	popOutTheItem();
	// setRemoved
	level.levelEvent(nullptr, LEVEL_EVENT_JUKEBOX_STOP, pos(), 0);
}

void JukeboxBlockEntity::save(BlockEntityWriter& out) const {
	out.item(_item);
	out.u8(_song != nullptr);
	if (_song) out.varint(static_cast<uint32_t>(_ticksSinceSongStarted));
}

void JukeboxBlockEntity::load(BlockEntityReader& in) {
	_item = in.item();
	_song = nullptr;
	if (in.u8()) {
		int64_t ticks = in.varint();
		// setSongWithoutPlaying: unless it is over
		const JukeboxSong* song = JukeboxSong::fromStack(_item, in.gameData);
		if (song && !song->hasFinished(ticks)) {
			_song				   = song;
			_ticksSinceSongStarted = ticks;
		}
	}
}

// ===== Lectern =====

const GameData* LecternBlockEntity::data() const { return level() ? &level()->gameData() : _gameData; }

bool LecternBlockEntity::hasBook() const {
	if (_book.isEmpty() || !data()) return false;
	const GameData& gameData = *data();
	if (Components::get(_book, gameData, "minecraft:writable_book_content") || Components::get(_book, gameData, "minecraft:written_book_content")) return true;
	// The writable book's own component (an empty book), unless its patch removes it
	return gameData.getStaticName("minecraft:item", _book.item) == "minecraft:writable_book" &&
		   !Components::isRemoved(_book, gameData, "minecraft:writable_book_content");
}

int LecternBlockEntity::pageCount(const ItemStack& book) const {
	if (!data() || book.isEmpty()) return 0;
	const GameData& gameData = *data();
	if (std::optional<std::vector<uint8_t>> written = Components::get(book, gameData, "minecraft:written_book_content")) return Components::bookPageCount(*written, true);
	if (std::optional<std::vector<uint8_t>> writable = Components::get(book, gameData, "minecraft:writable_book_content")) {
		return Components::bookPageCount(*writable, false);
	}
	return 0;
}

// setBook: open at the first page (resolving a written book's components isn't ported)
void LecternBlockEntity::setBook(ItemStack book) {
	_book	   = book.count > 0 ? std::move(book) : ItemStack();
	_page	   = 0;
	_pageCount = pageCount(_book);
	markChanged();
}

void LecternBlockEntity::setPage(int page) {
	// Mth.clamp: min(max(page, 0), count - 1)
	int clamped = std::min(std::max(page, 0), _pageCount - 1);
	if (clamped == _page || !level()) return;
	_page = clamped;
	markChanged();
	// LecternBlock.signalPageChange: powered for 2 ticks
	Level&				 at	   = *level();
	const BlockRegistry& reg   = at.blocks();
	int					 state = at.getBlockState(pos());
	at.setBlock(pos(), reg.withBool(state, reg.property("powered"), true), Level::UPDATE_ALL);
	at.updateNeighborsAt(pos().below(), reg.blockOf(state));
	at.scheduleTick(pos(), reg.blockOf(state), 2);
	at.levelEvent(nullptr, 1043, pos(), 0); // LevelEvent.SOUND_... page turned (lectern)
}

int LecternBlockEntity::redstoneSignal() const {
	float progress = _pageCount > 1 ? _page / (_pageCount - 1.0F) : 1.0F;
	return static_cast<int>(std::floor(progress * 14.0F)) + (hasBook() ? 1 : 0);
}

void LecternBlockEntity::onBookItemRemove() {
	_page	   = 0;
	_pageCount = 0;
	if (!level()) return;
	// LecternBlock.resetBookState(false)
	Level&				 at	   = *level();
	const BlockRegistry& reg   = at.blocks();
	int					 state = at.getBlockState(pos());
	int					 reset = reg.withBool(reg.withBool(state, reg.property("powered"), false), reg.property("has_book"), false);
	at.setBlock(pos(), reset, Level::UPDATE_ALL);
	at.updateNeighborsAt(pos().below(), reg.blockOf(state));
}

ItemStack LecternBlockEntity::BookAccess::removeItem(int slot, int count) {
	if (slot != 0) return {};
	ItemStack taken = splitSlot(_lectern._book, count);
	if (_lectern._book.isEmpty()) _lectern.onBookItemRemove();
	return taken;
}

// removeItemNoUpdate
ItemStack LecternBlockEntity::BookAccess::takeBook() {
	ItemStack book = std::move(_lectern._book);
	_lectern._book = ItemStack();
	_lectern.onBookItemRemove();
	return book;
}

bool LecternBlockEntity::BookAccess::stillValid(Player& player) { return stillValidBlockEntity(_lectern, player) && _lectern.hasBook(); }

// Drops the book a quarter block toward its front, at the top, if the lectern had one
void LecternBlockEntity::preRemoveSideEffectsWithState(Level& level, int oldState) {
	const BlockRegistry& reg = level.blocks();
	if (!reg.getBool(oldState, reg.property("has_book"))) return;
	std::string facing	  = reg.valueName(reg.get(oldState, reg.property("facing")));
	Direction	direction = facing == "south" ? Direction::South : facing == "west" ? Direction::West : facing == "east" ? Direction::East : Direction::North;
	float		dx		  = 0.25F * step(direction, 0);
	float		dz		  = 0.25F * step(direction, 2);
	dropAt(level, pos().x + 0.5 + dx, pos().y + 1, pos().z + 0.5 + dz, _book);
}

void LecternBlockEntity::save(BlockEntityWriter& out) const {
	out.item(_book);
	if (!_book.isEmpty()) out.varint(static_cast<uint32_t>(std::max(_page, 0)));
}

void LecternBlockEntity::load(BlockEntityReader& in) {
	_gameData  = &in.gameData;
	_book	   = in.item();
	int page   = _book.isEmpty() ? 0 : static_cast<int>(in.varint());
	_pageCount = pageCount(_book);
	_page	   = std::min(std::max(page, 0), _pageCount - 1);
}

// ===== Crafter =====

void CrafterBlockEntity::setSlotState(int slot, bool enabled) {
	if (!slotCanBeDisabled(slot)) return;
	_slotStates[slot] = enabled ? 0 : 1;
	markChanged();
}

void CrafterBlockEntity::setItem(int slot, ItemStack stack) {
	if (isSlotDisabled(slot)) setSlotState(slot, true);
	ContainerBlockEntity::setItem(slot, std::move(stack));
}

bool CrafterBlockEntity::smallerStackExist(int count, const ItemStack& stack, int slot) const {
	for (int i = slot + 1; i < 9; i++) {
		if (isSlotDisabled(i)) continue;
		const ItemStack& other = _items[i];
		if (other.isEmpty() || (other.count < count && other.sameItemSameComponents(stack))) return true;
	}
	return false;
}

// canPlaceItem: hoppers fill the grid evenly
bool CrafterBlockEntity::canPlaceItem(int slot, const ItemStack&) const {
	if (_slotStates.at(slot) == 1) return false;
	const ItemStack& there = _items[slot];
	if (there.isEmpty()) return true;
	const GameData::ItemProperties* item = level() ? level()->gameData().getItemProperties(there.item) : nullptr;
	if (there.count >= (item ? item->maxStackSize : 64)) return false;
	return !smallerStackExist(there.count, there, slot);
}

void CrafterBlockEntity::tick(Level& level) {
	int left = _craftingTicksRemaining - 1;
	if (left < 0) return;
	_craftingTicksRemaining = left;
	if (left == 0) {
		int					 state = level.getBlockState(pos());
		const BlockRegistry& reg   = level.blocks();
		if (reg.has(state, reg.property("crafting"))) level.setBlock(pos(), reg.withBool(state, reg.property("crafting"), false), Level::UPDATE_ALL);
	}
}

int CrafterBlockEntity::redstoneSignal() {
	int count = 0;
	for (int i = 0; i < 9; i++) count += !_items[i].isEmpty() || isSlotDisabled(i);
	return count;
}

void CrafterBlockEntity::saveExtra(BlockEntityWriter& out) const {
	out.varint(static_cast<uint32_t>(_craftingTicksRemaining));
	uint32_t disabled = 0;
	for (int i = 0; i < 9; i++) disabled |= static_cast<uint32_t>(isSlotDisabled(i)) << i;
	out.varint(disabled);
	out.u8(static_cast<uint8_t>(_triggered));
}

void CrafterBlockEntity::loadExtra(BlockEntityReader& in) {
	_craftingTicksRemaining = static_cast<int>(in.varint());
	uint32_t disabled		= in.varint();
	for (int i = 0; i < 9; i++) _slotStates[i] = (disabled >> i & 1) && slotCanBeDisabled(i) ? 1 : 0;
	_triggered = in.u8();
}
