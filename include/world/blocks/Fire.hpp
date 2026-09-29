#ifndef FIRE_HPP
#define FIRE_HPP

#include "world/BlockBehavior.hpp"
#include "world/blocks/BlockContext.hpp"

#include <memory>
#include <string>
#include <vector>

// Fire (vanilla's BaseFireBlock, FireBlock, SoulFireBlock): burns entities inside, lights nether portals when placed
// in an obsidian frame, spreads to flammable blocks and burns them out, goes out in the rain

class BaseFireBlock : public BlockBehavior {
  public:
	BaseFireBlock(std::shared_ptr<const BlockContext> context, float fireDamage);

	// BaseFireBlock.getState: soul fire on soul sand and soul soil, else fire facing what burns around
	static int	getState(Level& level, const BlockPos& pos);
	// BaseFireBlock.canBePlacedAt: flint and steel and fire charges light a fire there (air where fire survives, or
	// inside an obsidian frame that makes a portal)
	static bool canBePlacedAt(Level& level, const BlockPos& pos, Direction direction);
	// BaseFireBlock.fireIgnite: sets an entity on fire (8 seconds)
	static void fireIgnite(Level& level, Actor& actor);

	void entityInside(Level& level, const BlockPos& pos, int state, Actor* actor) const override;
	void onPlace(Level& level, const BlockPos& pos, int state, int oldState, bool movedByPiston) const override;
	void playerWillDestroy(Level& level, const BlockPos& pos, int state, Player& player) const override;
	// Fire hurts what is inside: the damage of this kind of fire (1 for fire, 2 for soul fire)
	float fireDamage() const { return _fireDamage; }

  protected:
	std::shared_ptr<const BlockContext> _context;
	float								_fireDamage;
	int									_air;
};

class FireBlock : public BaseFireBlock {
  public:
	struct Flammability {
		const char* block;
		int			igniteOdds, burnOdds;
	};
	// FireBlock.bootStrap: the blocks that burn (src/world/blocks/FireTable.cpp)
	static const std::vector<Flammability>& flammabilityTable();

	explicit FireBlock(std::shared_ptr<const BlockContext> context);

	// FireBlock.getStateForPlacement(level, pos): on a block that doesn't burn nor hold it, clinging to the sides that burn
	int	 getStateAt(Level& level, const BlockPos& pos) const;
	bool canBurn(int state) const { return igniteOdds(state) > 0; }
	int	 getStateForPlacement(Level& level, const PlaceContext& context) const override;
	int	 updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
	bool canSurvive(Level& level, const BlockPos& pos, int state) const override;
	void tick(Level& level, const BlockPos& pos, int state) const override;
	void onPlace(Level& level, const BlockPos& pos, int state, int oldState, bool movedByPiston) const override;

  private:
	std::vector<int> _igniteOdds, _burnOdds; // By block id
	int				 _age, _faces[6];		 // Properties (faces by Direction, -1 for down)
	int				 _tnt;

	int	 igniteOdds(int state) const;
	int	 burnOdds(int state) const;
	int	 getStateWithAge(Level& level, const BlockPos& pos, int age) const;
	bool isValidFireLocation(Level& level, const BlockPos& pos) const;
	int	 igniteOddsAround(Level& level, const BlockPos& pos) const;
	bool isNearRain(Level& level, const BlockPos& pos) const;
	void checkBurnOut(Level& level, const BlockPos& pos, int chance, int age) const;
};

class SoulFireBlock : public BaseFireBlock {
  public:
	explicit SoulFireBlock(std::shared_ptr<const BlockContext> context) : BaseFireBlock(std::move(context), 2.0F) {}
	int	 updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
	bool canSurvive(Level& level, const BlockPos& pos, int state) const override;
	// SoulFireBlock.canSurviveOnBlock: soul sand, soul soil (#soul_fire_base_blocks)
	static bool canSurviveOnBlock(Level& level, int belowState);
};

#endif
