#ifndef VEGETATION_HPP
#define VEGETATION_HPP

#include "world/BlockBehavior.hpp"
#include "world/blocks/BlockContext.hpp"

#include <memory>
#include <vector>

// Vanilla's VegetationBlock and subclasses: plants that break (without a scheduled tick) when the block below
// can't hold them anymore. What they may stand on (mayPlaceOn) depends on the class
class VegetationBlock : public BlockBehavior {
  public:
	enum class Soil {
		Dirt,		   // VegetationBlock: dirt tag or farmland (flowers, grass, saplings, bushes...)
		DirtOrClay,	   // AzaleaBlock, MangrovePropaguleBlock
		Nether,		   // RootsBlock, NetherSproutsBlock: + nylium, soul soil
		Fungus,		   // FungusBlock: + nylium, mycelium, soul soil
		WitherRose,	   // WitherRoseBlock: + netherrack, soul sand, soul soil
		Farmland,	   // CropBlock, StemBlock, AttachedStemBlock
		SoulSand,	   // NetherWartBlock
		DryVegetation, // DryVegetationBlock: dry_vegetation_may_place_on tag
		CactusFlower,  // CactusFlowerBlock: cactus, farmland, or a top that holds its center
		Mushroom,	   // MushroomBlock: an opaque full block
		Seagrass,	   // SeagrassBlock, TallSeagrassBlock: sturdy top, not magma
		Waterlily,	   // WaterlilyBlock: on water or ice, nothing liquid above
		SeaPickle,	   // SeaPickleBlock: any top face, or a sturdy one
		SturdyTop,	   // LeafLitterBlock (its canSurvive)
	};

	VegetationBlock(std::shared_ptr<const BlockContext> context, Soil soil);

	int			 updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
	bool canSurvive(Level& level, const BlockPos& pos, int state) const override;

  protected:
	std::shared_ptr<const BlockContext> _context;
	Soil								_soil;
	std::vector<bool>					_dirt, _nylium, _dryVegetationSoil, _mushroomGrowBlocks;
	int _farmland, _clay, _soulSoil, _soulSand, _mycelium, _netherrack, _cactus, _magma, _mangroveLeaves;

	bool mayPlaceOn(Level& level, const BlockPos& belowPos, int below) const;
};

// CropBlock (wheat, carrots, potatoes, beetroots, torchflower): farmland, and light 8 or more. Grows with light 9 or
// more, faster on wet farmland and slower when crops of the same kind are packed together
class CropBlock : public VegetationBlock {
  public:
	enum class Kind { Crop, Beetroot, Torchflower };

	CropBlock(std::shared_ptr<const BlockContext> context, int block, Kind kind);
	bool canSurvive(Level& level, const BlockPos& pos, int state) const override;
	void randomTick(Level& level, const BlockPos& pos, int state) const override;
	// CropBlock.getGrowthSpeed, also used by stems
	static float growthSpeed(const BlockContext& context, Level& level, int block, const BlockPos& pos, int farmland);

  private:
	Kind _kind;
	int	 _maxAge;
	int	 _torchflower; // What a torchflower crop becomes
};

// StemBlock (pumpkin, melon): grows like a crop, then puts its fruit next to it
class StemBlock : public VegetationBlock {
  public:
	StemBlock(std::shared_ptr<const BlockContext> context, int fruit, int attachedStem);
	void randomTick(Level& level, const BlockPos& pos, int state) const override;

  private:
	int				  _fruit, _attachedStem;
	std::vector<bool> _dirtTag;
};

// NetherWartBlock, SweetBerryBushBlock: an age that goes up now and then
class NetherWartBlock : public VegetationBlock {
  public:
	explicit NetherWartBlock(std::shared_ptr<const BlockContext> context) : VegetationBlock(std::move(context), Soil::SoulSand) {}
	void randomTick(Level& level, const BlockPos& pos, int state) const override;
};

class SweetBerryBushBlock : public VegetationBlock {
  public:
	explicit SweetBerryBushBlock(std::shared_ptr<const BlockContext> context) : VegetationBlock(std::move(context), Soil::Dirt) {}
	void randomTick(Level& level, const BlockPos& pos, int state) const override;
};

// MushroomBlock: anything in mushroom_grow_block, or an opaque block in light under 13. Spreads slowly around
class MushroomBlock : public VegetationBlock {
  public:
	explicit MushroomBlock(std::shared_ptr<const BlockContext> context) : VegetationBlock(std::move(context), Soil::Mushroom) {}
	bool canSurvive(Level& level, const BlockPos& pos, int state) const override;
	void randomTick(Level& level, const BlockPos& pos, int state) const override;
};

// Grows a tree at a sapling's position (TreeGrower.growTree): returns whether it did
class TreeGrower {
  public:
	virtual ~TreeGrower()											  = default;
	virtual bool growTree(Level& level, const BlockPos& pos, int sapling) const = 0;
};

// SaplingBlock: stage 0, stage 1, then a tree, with light 9 or more
class SaplingBlock : public VegetationBlock {
  public:
	SaplingBlock(std::shared_ptr<const BlockContext> context, Soil soil, std::shared_ptr<const TreeGrower> grower);
	void randomTick(Level& level, const BlockPos& pos, int state) const override;
	void advanceTree(Level& level, const BlockPos& pos, int state) const;

  protected:
	std::shared_ptr<const TreeGrower> _grower;
	int								  _stage;
};

// SeagrassBlock: its water flows again after the check
class SeagrassBlock : public VegetationBlock {
  public:
	explicit SeagrassBlock(std::shared_ptr<const BlockContext> context) : VegetationBlock(std::move(context), Soil::Seagrass) {}
	int updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
};

// SeaPickleBlock: waterlogged, its water flows again when it survives
class SeaPickleBlock : public VegetationBlock {
  public:
	explicit SeaPickleBlock(std::shared_ptr<const BlockContext> context) : VegetationBlock(std::move(context), Soil::SeaPickle) {}
	int updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
};

// MangrovePropaguleBlock: hanging ones need mangrove leaves above, and grow up to age 4; the others are saplings
// (without the light check)
class MangrovePropaguleBlock : public SaplingBlock {
  public:
	MangrovePropaguleBlock(std::shared_ptr<const BlockContext> context, std::shared_ptr<const TreeGrower> grower)
		: SaplingBlock(std::move(context), Soil::DirtOrClay, std::move(grower)) {}
	int	 updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
	bool canSurvive(Level& level, const BlockPos& pos, int state) const override;
	void randomTick(Level& level, const BlockPos& pos, int state) const override;
};

// AttachedStemBlock: turns back into a grown stem when its fruit (pumpkin, melon) goes
class AttachedStemBlock : public VegetationBlock {
  public:
	AttachedStemBlock(std::shared_ptr<const BlockContext> context, int fruit, int stem);
	int updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;

  private:
	int _fruit, _stem;
};

// DoublePlantBlock (tall grass, large fern, tall flowers, tall seagrass): two halves that break together
class DoublePlantBlock : public VegetationBlock {
  public:
	DoublePlantBlock(std::shared_ptr<const BlockContext> context, Soil soil, bool seagrass);
	int	 updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
	bool canSurvive(Level& level, const BlockPos& pos, int state) const override;

  private:
	bool _seagrass; // TallSeagrassBlock: the lower half also needs full water
};

#endif
