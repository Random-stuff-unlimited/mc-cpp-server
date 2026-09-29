#ifndef EXPLOSION_HPP
#define EXPLOSION_HPP

#include "world/BlockBehavior.hpp"
#include "world/BlockPos.hpp"
#include "world/Combat.hpp"
#include "world/Fluids.hpp"
#include "world/blocks/BlockContext.hpp"
#include "world/entity/Geometry.hpp"
#include "world/item/ItemStack.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class Actor;
class Level;

// Explosions (vanilla's ServerExplosion, ExplosionDamageCalculator, Level.explode): rays from the center break the
// blocks their power gets through, the entities around are hurt by how much of them the center sees and pushed away,
// the blocks drop their loot (merged), fire may be left behind, and each player within 64 blocks gets the explosion
// packet (with its own knockback)
namespace Explosions {
	// Level.ExplosionInteraction
	enum class Interaction { None, Block, Mob, Tnt, Trigger };
	// Explosion.BlockInteraction
	enum class BlockInteraction { Keep, Destroy, DestroyWithDecay, TriggerBlock };

	// ExplosionDamageCalculator
	class Calculator {
	  public:
		virtual ~Calculator() = default;
		// The resistance of the block and its fluid, nothing for air
		virtual std::optional<float> blockExplosionResistance(Level& level, const BlockPos& pos, int state, const FluidState& fluid) const;
		virtual bool				 shouldBlockExplode(Level&, const BlockPos&, int, float) const { return true; }
		virtual bool				 shouldDamageEntity(Actor&) const { return true; }
		virtual float				 knockbackMultiplier(Actor&) const { return 1.0F; }
	};

	struct Explosion {
		Level&					level;
		Actor*					source; // The entity that exploded (primed TNT, creeper, fireball), if any
		Combat::DamageSource	damage;
		const Calculator&		calculator;
		Vec3					center;
		float					radius;
		bool					fire;
		BlockInteraction		interaction;

		// Explosion.getIndirectSourceEntity: who is responsible (the one that lit the TNT, the creeper itself)
		Actor* indirectSource() const;
	};

	// Level.explode with the default particles and sound (SoundEvents.GENERIC_EXPLODE): damage defaults to
	// Explosion.getDefaultDamageSource, calculator to the source's own
	void explode(Level& level, Actor* source, std::optional<Combat::DamageSource> damage, const Calculator* calculator, const Vec3& center, float radius,
				 bool fire, Interaction interaction);
	// ServerExplosion.getSeenPercent: how much of the box the center sees, 0 to 1
	float seenPercent(Level& level, const Vec3& center, const AABB& box);
	// TntBlock.prime: a primed TNT at pos, lit by owner (null if none). False if TNT doesn't explode (never here)
	bool primeTnt(Level& level, const BlockPos& pos, Actor* owner);
} // namespace Explosions

// TntBlock: primed by redstone, fire, flint and steel, fire charges, burning arrows and explosions
class TntBlock : public BlockBehavior {
  public:
	explicit TntBlock(std::shared_ptr<const BlockContext> context);
	void	  onPlace(Level& level, const BlockPos& pos, int state, int oldState, bool movedByPiston) const override;
	void	  neighborChanged(Level& level, const BlockPos& pos, int state, int sourceBlock, bool movedByPiston) const override;
	void	  playerWillDestroy(Level& level, const BlockPos& pos, int state, Player& player) const override;
	UseResult useItemOn(Level& level, const BlockPos& pos, int state, Player& player, int hand, const BlockHit& hit) const override;
	bool	  dropFromExplosion() const override { return false; }
	void	  wasExploded(Level& level, const BlockPos& pos, Explosions::Explosion& explosion) const override;

  private:
	std::shared_ptr<const BlockContext> _context;
	int									_unstable;
};

#endif
