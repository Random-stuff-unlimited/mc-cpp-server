#ifndef ACTOR_HPP
#define ACTOR_HPP

#include "lib/UUID.hpp"
#include "world/BlockPos.hpp"
#include "world/entity/Geometry.hpp"

class Entity;
class Level;
class LivingEntity;
class Player;
namespace Combat {
	struct DamageSource;
}

// Vanilla's Entity as the game logic sees it: a player or any other entity. Players and the other entities are two
// classes here (Player, Entity); what mobs, projectiles, explosions and damage need from either goes through this.
// A reference kept across ticks is an EntityRef (id and UUID), resolved when used: entity ids are reused
class Actor {
  public:
	// Entity.portalCooldown and its PortalProcessor: the portal it stands in and for how long
	struct PortalState {
		int		 cooldown = 0;
		bool	 active	  = false; // A portal is being processed (portalProcess != null)
		int		 kind	  = 0;	   // Portals::Kind
		BlockPos entry;
		int		 portalTime		= 0;
		bool	 insideThisTick = false;
	};
	PortalState portal;

	virtual ~Actor() = default;
	// Entity.getDimensionChangingDelay: the portal cooldown after changing dimension (300 ticks, 10 for players)
	virtual int dimensionChangingDelay() const { return 300; }

	virtual int			id() const		 = 0; // Entity.getId (shared by players and entities)
	virtual const UUID& uuid() const	 = 0;
	virtual int			typeId() const	 = 0; // minecraft:entity_type id
	virtual Level*		actorLevel() const = 0; // The level it is in, null for a player not in game yet
	virtual const Vec3& position() const = 0;
	virtual AABB		boundingBox() const = 0;
	// Entity.getEyeY
	virtual double		eyeY() const = 0;
	// Entity.isAlive: not removed, and for living ones, not dead
	virtual bool		isAlive() const = 0;
	virtual bool		isSpectator() const { return false; }
	// Entity.hurtServer: damage from a source. Returns whether it hurt
	virtual bool		hurtServer(const Combat::DamageSource& source, float amount) = 0;
	// Entity.push(Vec3): adds to its movement (for a player, its client is told)
	virtual void		pushMotion(const Vec3& impulse) = 0;
	// Entity.getYRot
	virtual float		yRot() const = 0;
	// Entity.igniteForTicks: burns at least this long
	virtual void		igniteForTicks(int ticks) = 0;

	virtual Player*		  asPlayer() { return nullptr; }
	virtual Entity*		  asEntity() { return nullptr; }
	virtual LivingEntity* asLiving() { return nullptr; }
	const Player*		  asPlayer() const { return const_cast<Actor*>(this)->asPlayer(); }
	const LivingEntity*	  asLiving() const { return const_cast<Actor*>(this)->asLiving(); }
	bool				  isPlayer() const { return const_cast<Actor*>(this)->asPlayer() != nullptr; }

	Vec3   eyePosition() const { return {position().x, eyeY(), position().z}; }
	double distanceToSqr(const Vec3& point) const { return (position() - point).lengthSqr(); }
	double distanceToSqr(const Actor& other) const { return distanceToSqr(other.position()); }
};

// A reference to an actor kept across ticks (vanilla's EntityReference): its id to find it, its UUID to be sure it
// is still the same one
struct EntityRef {
	int	 id = -1;
	UUID uuid{};

	EntityRef() = default;
	explicit EntityRef(const Actor& actor) : id(actor.id()), uuid(actor.uuid()) {}
	bool isSet() const { return id != -1; } // -2: loaded from a save, found by its UUID
	void clear() { id = -1; }
	bool operator==(const EntityRef& other) const { return id == other.id && uuid == other.uuid; }
};

#endif
