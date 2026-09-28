#ifndef TREES_HPP
#define TREES_HPP

#include "lib/JavaHashSet.hpp"
#include "lib/json.hpp"
#include "world/BlockPos.hpp"
#include "world/blocks/Vegetation.hpp"

#include <cmath>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class GameData;
class JavaRandom;
class Level;
struct BlockContext;

// TreeFeature.BLOCK_UPDATE_FLAGS: Level::UPDATE_ALL | Level::UPDATE_KNOWN_SHAPE
constexpr int TREE_FLAGS = 19;

// Mth.floor, with Java's (int) of NaN (0)
inline int javaFloor(double value) {
	if (std::isnan(value)) return 0;
	int truncated = static_cast<int>(value);
	return value < truncated ? truncated - 1 : truncated;
}

// The blocks, tags and properties trees look at, looked up once for all of them
struct TreeBlocks {
	std::vector<bool> replaceableByTrees, logs, leavesTag, dirt, leavesBlocks; // leavesBlocks: LeavesBlock instances
	int				  vine, grass, mycelium, beeNest;
	int				  axis, axes[3], persistent, distance;

	explicit TreeBlocks(const BlockContext& context);
};

// Vanilla's TreeFeature: a tree of the game's data (tree_features.json: configured features of type minecraft:tree),
// placed block by block like vanilla (trunk, foliage, decorators, then the leaves' distances), with the same random
// draws. Trees using a placer or decorator not ported yet (mangroves, azaleas, pale oaks...) aren't supported
class TreeFeature {
  public:
	TreeFeature(const nlohmann::json& config, const BlockContext& context, const TreeBlocks& blocks);

	bool supported() const { return _supported; }
	// Feature.place: false if there is no room for it (nothing placed then)
	bool place(Level& level, JavaRandom& random, const BlockPos& origin) const;

	// IntProvider: constant, uniform or weighted list
	struct IntProvider {
		int										 min = 0, max = 0; // Constant: min == max
		std::vector<std::pair<int, IntProvider>> weighted;		   // Weight, value
		int										 totalWeight = 0;
		static IntProvider						 parse(const nlohmann::json& value);
		int										 sample(JavaRandom& random) const;
	};

  private:
	struct Attachment {
		BlockPos pos;
		int		 radiusOffset;
		bool	 doubleTrunk;
	};

	// Everything placed by one tree, in vanilla's hash sets (their order decides a few things)
	struct Placement {
		Level&					 level;
		JavaRandom&				 random;
		JavaHashSet<BlockPos>	 roots, trunk, foliage, decorations;

		void set(JavaHashSet<BlockPos>& into, const BlockPos& pos, int state);
	};

	enum class TrunkType { Straight, Forking, Giant, MegaJungle, DarkOak, Fancy, Cherry };
	enum class FoliageType { Blob, Fancy, Spruce, Pine, MegaPine, Acacia, DarkOak, Jungle, Cherry };
	enum class DecoratorType { Beehive, AlterGround, TrunkVine, LeaveVine };
	struct Decorator {
		DecoratorType type;
		float		  probability = 0;
		int			  state		  = 0; // AlterGround's block
	};

	const BlockContext& _context;
	const TreeBlocks&	_ids;
	bool				_supported = true;

	// Block state providers: simple, or weighted (a random state)
	struct StateProvider {
		std::vector<std::pair<int, int>> states; // Weight, state
		int								 totalWeight = 0;
		int								 get(JavaRandom& random) const;
	};
	StateProvider _trunkProvider, _foliageProvider, _dirtProvider;
	bool		  _forceDirt, _ignoreVines;

	// FeatureSize: two or three layers
	int _sizeLimit, _sizeUpperLimit, _lowerSize, _middleSize, _upperSize, _minClippedHeight;
	bool _threeLayers;

	TrunkType	_trunk;
	int			_baseHeight, _heightRandA, _heightRandB;
	IntProvider _branchCount, _branchHorizontalLength, _branchStart, _secondBranchStart, _branchEnd; // Cherry

	FoliageType _foliage;
	IntProvider _radius, _offset, _foliageHeightProvider;
	int			_foliageHeight = 0;
	float		_wideBottomLayerHoleChance = 0, _cornerHoleChance = 0, _hangingLeavesChance = 0, _hangingLeavesExtensionChance = 0;

	std::vector<Decorator> _decorators;

	// Block questions of TreeFeature and the placers
	bool validTreePos(Level& level, const BlockPos& pos) const;
	bool isFree(Level& level, const BlockPos& pos) const;
	int	 sizeAtHeight(int height, int y) const;
	int	 maxFreeTreeHeight(Level& level, int height, const BlockPos& origin) const;

	// Trunk placers
	std::vector<Attachment> placeTrunk(Placement& p, int height, const BlockPos& origin) const;
	void					setDirtAt(Placement& p, const BlockPos& pos) const;
	bool					placeLog(Placement& p, const BlockPos& pos, int axis = -1) const;
	void					placeLogIfFree(Placement& p, const BlockPos& pos) const;
	bool					makeLimb(Placement& p, const BlockPos& start, const BlockPos& end, bool place) const;
	Attachment cherryBranch(Placement& p, int height, const BlockPos& origin, Direction direction, int start, bool trunkAbove, int axis) const;

	// Foliage placers
	int	 foliageHeight(JavaRandom& random, int height) const;
	int	 foliageRadius(JavaRandom& random, int trunkHeight) const;
	void createFoliage(Placement& p, int trunkHeight, const Attachment& attachment, int foliageHeight, int radius, int offset) const;
	void placeLeavesRow(Placement& p, const BlockPos& origin, int radius, int y, bool doubleTrunk) const;
	void placeLeavesRowWithHangingLeavesBelow(Placement& p, const BlockPos& origin, int radius, int y, bool doubleTrunk, float chance,
											  float extensionChance) const;
	bool tryPlaceLeaf(Placement& p, const BlockPos& pos) const;
	bool tryPlaceExtension(Placement& p, float chance, const BlockPos& logPos, const BlockPos& pos) const;
	bool shouldSkipLocationSigned(JavaRandom& random, int dx, int y, int dz, int radius, bool doubleTrunk) const;
	bool shouldSkipLocation(JavaRandom& random, int dx, int y, int dz, int radius, bool doubleTrunk) const;

	// Decorators and the leaves' distances
	void decorate(Placement& p) const;
	void updateLeaves(Placement& p) const;
};

// The trees of the game's data, by name ("minecraft:oak")
class TreeFeatures {
  public:
	TreeFeatures(const std::filesystem::path& file, const BlockContext& context);
	const TreeFeature* get(const std::string& name) const;

  private:
	TreeBlocks													  _blocks;
	std::unordered_map<std::string, std::unique_ptr<TreeFeature>> _trees;
};

// Vanilla's TreeGrower: the tree a sapling becomes (2x2 saplings: a mega tree; flowers around: the variant with bees)
class SaplingTreeGrower : public TreeGrower {
  public:
	struct Config {
		float		secondaryChance = 0.0F;
		std::string megaTree, secondaryMegaTree, tree, secondaryTree, flowers, secondaryFlowers; // Empty: none
	};
	// Vanilla's grower of a sapling block ("minecraft:oak_sapling"), nullptr if it has none
	static std::shared_ptr<const SaplingTreeGrower> forSapling(const std::string& sapling, std::shared_ptr<const TreeFeatures> trees,
															   std::shared_ptr<const BlockContext> context);

	SaplingTreeGrower(Config config, std::shared_ptr<const TreeFeatures> trees, std::shared_ptr<const BlockContext> context);
	bool growTree(Level& level, const BlockPos& pos, int sapling) const override;

  private:
	Config								_config;
	std::shared_ptr<const TreeFeatures> _trees;
	std::shared_ptr<const BlockContext> _context;
	std::vector<bool>					_flowers;
	int									_air;

	bool hasFlowers(Level& level, const BlockPos& pos) const;
};

#endif
