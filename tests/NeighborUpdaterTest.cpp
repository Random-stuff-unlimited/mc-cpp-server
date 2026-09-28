#include "Test.hpp"
#include "world/NeighborUpdater.hpp"

#include <functional>
#include <string>
#include <vector>

namespace {
	std::string name(const BlockPos& pos) { return std::to_string(pos.x) + "," + std::to_string(pos.y) + "," + std::to_string(pos.z); }

	// Records the updates; `react` lets a test trigger more updates from inside one, like a block would
	struct Recorder : NeighborUpdateTarget {
		std::vector<std::string>										   log;
		std::function<void(NeighborUpdater&, const BlockPos&, int source)> react;
		NeighborUpdater*												   updater = nullptr;

		int	 getBlockState(const BlockPos&) override { return 0; }
		void executeNeighborChanged(int, const BlockPos& pos, int source, bool) override {
			log.push_back(name(pos));
			if (react) react(*updater, pos, source);
		}
		void executeShapeUpdate(Direction, const BlockPos& pos, const BlockPos&, int, int, int) override { log.push_back("shape " + name(pos)); }
	};
} // namespace

// NeighborUpdater.UPDATE_ORDER: west, east, down, up, north, south
TEST(neighbor_updates_order) {
	Recorder		r;
	NeighborUpdater updater(r);
	r.updater = &updater;
	updater.updateNeighborsAtExceptFromFacing({0, 0, 0}, 1, nullptr);
	CHECK(r.log == (std::vector<std::string>{"-1,0,0", "1,0,0", "0,-1,0", "0,1,0", "0,0,-1", "0,0,1"}));

	r.log.clear();
	Direction skip = Direction::Down;
	updater.updateNeighborsAtExceptFromFacing({0, 0, 0}, 1, &skip);
	CHECK(r.log == (std::vector<std::string>{"-1,0,0", "1,0,0", "0,1,0", "0,0,-1", "0,0,1"}));
}

// An update requested during another runs before the rest of the current batch (depth first)
TEST(neighbor_updates_depth_first) {
	Recorder		r;
	NeighborUpdater updater(r);
	r.updater = &updater;
	r.react	  = [](NeighborUpdater& u, const BlockPos& pos, int source) {
		  if (source == 1 && pos == BlockPos{-1, 0, 0}) u.updateNeighborsAtExceptFromFacing(pos, 2, nullptr);
	};
	updater.updateNeighborsAtExceptFromFacing({0, 0, 0}, 1, nullptr);
	std::vector<std::string> expected = {"-1,0,0",											   // Origin's west...
										 "-2,0,0", "0,0,0", "-1,-1,0", "-1,1,0", "-1,0,-1", "-1,0,1", // ...updates its six neighbors first
										 "1,0,0",  "0,-1,0", "0,1,0", "0,0,-1", "0,0,1"};		   // Then the origin's other neighbors
	CHECK(r.log == expected);
}

// Several updates added by the same step run in the order they were added
TEST(neighbor_updates_added_in_order) {
	Recorder		r;
	NeighborUpdater updater(r);
	r.updater = &updater;
	r.react	  = [](NeighborUpdater& u, const BlockPos& pos, int source) {
		  if (source == 1 && pos == BlockPos{-1, 0, 0}) {
			  u.neighborChanged({10, 0, 0}, 2);
			  u.shapeUpdate(Direction::Up, 0, {11, 0, 0}, {11, 1, 0}, 0, 512);
			  u.neighborChanged({12, 0, 0}, 2);
		  }
	};
	updater.updateNeighborsAtExceptFromFacing({0, 0, 0}, 1, nullptr);
	std::vector<std::string> expected = {"-1,0,0", "10,0,0", "shape 11,0,0", "12,0,0", "1,0,0", "0,-1,0", "0,1,0", "0,0,-1", "0,0,1"};
	CHECK(r.log == expected);
}

// Past the limit of chained updates, the rest is skipped; the next chain starts from zero
TEST(neighbor_updates_chain_limit) {
	Recorder		r;
	NeighborUpdater updater(r, 10);
	r.updater = &updater;
	r.react	  = [](NeighborUpdater& u, const BlockPos& pos, int) { u.neighborChanged(pos.relative(Direction::East), 1); }; // Endless chain
	updater.neighborChanged({0, 0, 0}, 1);
	CHECK_EQ(r.log.size(), size_t(10));
	r.react = nullptr;
	r.log.clear();
	updater.neighborChanged({0, 0, 0}, 1);
	CHECK_EQ(r.log.size(), size_t(1));
}
