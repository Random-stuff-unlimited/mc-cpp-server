#include "Test.hpp"
#include "world/Chunk.hpp"
#include "world/LevelTicks.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace {
	// Scheduled ticks over a few chunks, with a subTickOrder counter like Level's
	struct Ticks {
		std::map<int64_t, std::unique_ptr<ChunkTicks>> chunks;
		std::map<int64_t, bool>						   ticking;
		LevelTicks									   ticks{[this](int64_t key) { return ticking[key]; }};
		int64_t										   subTick = 0;
		std::vector<std::string>					   ran;

		ChunkTicks& chunk(int x, int z) {
			int64_t key = Chunk::key(x, z);
			if (!chunks.count(key)) {
				chunks[key]	 = std::make_unique<ChunkTicks>();
				ticking[key] = true;
				ticks.addContainer(key, chunks[key].get());
			}
			return *chunks[key];
		}
		void schedule(BlockPos pos, int type, int64_t trigger, int priority = NORMAL) {
			chunk(pos.chunkX(), pos.chunkZ());
			ticks.schedule({type, pos, trigger, priority, subTick++});
		}
		void run(int64_t gameTime, int max = 65536, std::function<void(const BlockPos&, int)> extra = nullptr) {
			ticks.tick(gameTime, max, [&](const BlockPos& pos, int type) {
				ran.push_back(std::to_string(type) + "@" + std::to_string(pos.x));
				if (extra) extra(pos, type);
			});
		}
	};
} // namespace

// Within a chunk: DRAIN_ORDER (trigger tick, priority, scheduling order). Between chunks, vanilla only compares the
// priority and scheduling order of their next tick (INTRA_TICK_DRAIN_ORDER): a late tick of another chunk doesn't
// run first
TEST(level_ticks_run_in_vanilla_order) {
	Ticks t;
	t.schedule({1, 0, 0}, 1, 10);
	t.schedule({40, 0, 0}, 2, 10);			  // Other chunk, scheduled after
	t.schedule({2, 0, 0}, 3, 10, HIGH);		  // Higher priority runs first
	t.schedule({41, 0, 0}, 4, 9);			  // Earlier trigger (late: runs now too, first)
	t.schedule({3, 0, 0}, 5, 11);			  // Not yet
	t.run(10);
	std::vector<std::string> expected = {"3@2", "1@1", "4@41", "2@40"};
	CHECK(t.ran == expected);
	t.ran.clear();
	t.run(11);
	CHECK(t.ran == std::vector<std::string>{"5@3"});
}

TEST(level_ticks_one_per_position_and_type) {
	Ticks t;
	t.schedule({5, 0, 0}, 1, 3);
	t.schedule({5, 0, 0}, 1, 1); // Ignored: the position already has a tick of this type
	t.schedule({5, 0, 0}, 2, 1); // Other type: kept
	CHECK(t.ticks.hasScheduledTick({5, 0, 0}, 1));
	t.run(1);
	CHECK(t.ran == std::vector<std::string>{"2@5"});
	t.run(3);
	CHECK(t.ran == (std::vector<std::string>{"2@5", "1@5"}));
	CHECK(!t.ticks.hasScheduledTick({5, 0, 0}, 1));
}

// A tick scheduled while ticks run waits for the next game tick, even with delay 0
TEST(level_ticks_scheduled_during_run_wait) {
	Ticks t;
	t.schedule({1, 0, 0}, 1, 5);
	t.run(5, 65536, [&](const BlockPos&, int type) {
		if (type == 1) t.schedule({2, 0, 0}, 2, 5);
	});
	CHECK(t.ran == std::vector<std::string>{"1@1"});
	t.run(6);
	CHECK(t.ran == (std::vector<std::string>{"1@1", "2@2"}));
}

TEST(level_ticks_skip_chunks_not_ticking_and_cap) {
	Ticks t;
	t.schedule({1, 0, 0}, 1, 0);
	t.schedule({2, 0, 0}, 2, 0);
	t.schedule({3, 0, 0}, 3, 0);
	t.schedule({50, 0, 0}, 4, 0);
	t.ticking[Chunk::key(3, 0)] = false; // x = 50 is in chunk 3
	t.run(0, 2);						 // At most 2 per tick
	CHECK(t.ran == (std::vector<std::string>{"1@1", "2@2"}));
	t.run(1, 2);
	CHECK(t.ran == (std::vector<std::string>{"1@1", "2@2", "3@3"}));
	t.ticking[Chunk::key(3, 0)] = true;
	t.run(2, 2);
	CHECK(t.ran == (std::vector<std::string>{"1@1", "2@2", "3@3", "4@50"}));
}

// Saved ticks keep their delay and, once loaded, run before ticks scheduled afterwards at the same time
TEST(level_ticks_save_and_load) {
	Ticks t;
	t.schedule({1, 64, 0}, 7, 110, LOW);
	t.schedule({2, -5, 0}, 8, 105);
	std::vector<SavedTick> saved = t.chunk(0, 0).pack(100);
	CHECK_EQ(saved.size(), size_t(2));

	Ticks loaded;
	auto  chunk = std::make_unique<ChunkTicks>();
	chunk->setPending(saved);
	chunk->unpack(1000); // Loaded later: delays count from the new game time
	loaded.ticks.addContainer(Chunk::key(0, 0), chunk.get());
	loaded.ticking[Chunk::key(0, 0)] = true;
	loaded.chunks[Chunk::key(0, 0)]	 = std::move(chunk);
	loaded.schedule({3, 0, 0}, 9, 1005);
	loaded.run(1005);
	CHECK(loaded.ran == (std::vector<std::string>{"8@2", "9@3"}));
	loaded.run(1010);
	CHECK(loaded.ran == (std::vector<std::string>{"8@2", "9@3", "7@1"}));
}
