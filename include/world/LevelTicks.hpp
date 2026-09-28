#ifndef LEVEL_TICKS_HPP
#define LEVEL_TICKS_HPP

#include "world/BlockPos.hpp"

#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Scheduled ticks (a repeater switching 2 ticks later, water flowing 5 ticks later...), exactly like vanilla's
// LevelTicks / LevelChunkTicks: the order in which they run is what makes redstone timings (0-tick pistons...)
// behave the same.
//
// Ticks run in DRAIN_ORDER: trigger tick, then priority, then subTickOrder (a counter incremented by every
// schedule, so equal ticks run in scheduling order). A position has at most one tick of each type. At most
// maxTicks run per game tick, the rest waits for the next one. A tick scheduled while ticks run, even for the
// current game tick, runs at the next one.

// TickPriority: lower runs first
enum TickPriority : int8_t { EXTREMELY_HIGH = -3, VERY_HIGH = -2, HIGH = -1, NORMAL = 0, LOW = 1, VERY_LOW = 2, EXTREMELY_LOW = 3 };

struct ScheduledTick {
	int		 type = 0; // Block (or fluid) registry id
	BlockPos pos;
	int64_t	 triggerTick  = 0;
	int		 priority	  = NORMAL;
	int64_t	 subTickOrder = 0;

	bool operator==(const ScheduledTick& other) const {
		return type == other.type && pos == other.pos && triggerTick == other.triggerTick && priority == other.priority &&
			   subTickOrder == other.subTickOrder;
	}
};

// Ticks in a saved chunk: relative to the game time
struct SavedTick {
	int		 type;
	BlockPos pos;
	int		 delay;
	int		 priority;
};

class LevelTicks;

// LevelChunkTicks: the ticks of one chunk, saved with it. Modified on the game thread only; pack() may run on an
// I/O thread (saving), hence the lock
class ChunkTicks {
  public:
	ChunkTicks() = default;
	ChunkTicks(const ChunkTicks&)			 = delete;
	ChunkTicks& operator=(const ChunkTicks&) = delete;

	// Ticks read from disk, scheduled by unpack() once the game time is known
	void setPending(std::vector<SavedTick> ticks);
	void unpack(int64_t gameTime);

	const ScheduledTick* peek() const { return _queue.empty() ? nullptr : &_queue.front(); }
	ScheduledTick		 poll();
	// Ignored if the position already has a tick of this type
	void schedule(const ScheduledTick& tick);
	bool hasScheduledTick(const BlockPos& pos, int type) const;
	void removeIf(const std::function<bool(const ScheduledTick&)>& predicate);
	size_t count() const { return _queue.size() + _pending.size(); }
	std::vector<SavedTick> pack(int64_t gameTime) const;

  private:
	friend class LevelTicks;
	struct Key {
		int64_t pos;
		int		type;
		bool	operator==(const Key& other) const { return pos == other.pos && type == other.type; }
	};
	struct KeyHash {
		size_t operator()(const Key& key) const noexcept { return std::hash<int64_t>()(key.pos) * 31 + static_cast<size_t>(key.type); }
	};

	mutable std::mutex					_mutex;
	std::vector<ScheduledTick>			_queue; // Binary heap, earliest (DRAIN_ORDER) first
	std::unordered_set<Key, KeyHash>	_perPosition;
	std::vector<SavedTick>				_pending;
	LevelTicks*							_owner = nullptr; // Notified when a tick becomes the earliest
	void								scheduleUnchecked(const ScheduledTick& tick);
};

class LevelTicks {
  public:
	// tickCheck: whether ticks run in this chunk (key = Chunk::key) at the moment
	explicit LevelTicks(std::function<bool(int64_t)> tickCheck) : _tickCheck(std::move(tickCheck)) {}

	void addContainer(int64_t chunkKey, ChunkTicks* ticks);
	void removeContainer(int64_t chunkKey);
	// Returns false if the chunk isn't loaded
	bool schedule(const ScheduledTick& tick);
	void tick(int64_t gameTime, int maxTicks, const std::function<void(const BlockPos&, int)>& run);

	bool hasScheduledTick(const BlockPos& pos, int type) const;
	bool willTickThisTick(const BlockPos& pos, int type);

	static bool drainsBefore(const ScheduledTick& a, const ScheduledTick& b);	   // DRAIN_ORDER
	static bool intraTickBefore(const ScheduledTick& a, const ScheduledTick& b); // INTRA_TICK_DRAIN_ORDER

  private:
	friend class ChunkTicks;

	std::function<bool(int64_t)>			_tickCheck;
	std::unordered_map<int64_t, ChunkTicks*> _allContainers;
	std::unordered_map<int64_t, int64_t>	_nextTickForContainer;
	std::vector<ChunkTicks*>				_containersToTick; // Heap: container whose next tick runs first on top
	std::deque<ScheduledTick>				_toRunThisTick;
	std::vector<ScheduledTick>				_alreadyRunThisTick;
	std::unordered_set<ChunkTicks::Key, ChunkTicks::KeyHash> _toRunThisTickSet;

	void onTickAdded(ChunkTicks& container, const ScheduledTick& tick);
	void sortContainersToTick(int64_t gameTime);
	void drainContainers(int64_t gameTime, int maxTicks);
	void drainFromCurrentContainer(ChunkTicks& container, int64_t gameTime, int maxTicks);
	void updateContainerScheduling(const ScheduledTick& tick);
	void pushContainer(ChunkTicks* container);
	ChunkTicks* popContainer();
};

#endif
