#ifndef ATTRIBUTES_HPP
#define ATTRIBUTES_HPP

#include "data/GameData.hpp"

#include <deque>
#include <string>
#include <vector>

class Buffer;

// Ids (minecraft:attribute registry) of the attributes the game logic reads (vanilla's Attributes holders)
struct AttributeIds {
	int armor, armorToughness, attackDamage, attackKnockback, fallDamageMultiplier, followRange, gravity, jumpStrength, knockbackResistance,
			maxHealth, movementEfficiency, movementSpeed, oxygenBonus, safeFallDistance, scale, stepHeight, waterMovementEfficiency,
			explosionKnockbackResistance, attackSpeed, flyingSpeed, luck, spawnReinforcements, temptRange, burningTime, sweepingDamageRatio,
			miningEfficiency, blockBreakSpeed, submergedMiningSpeed, sneakingSpeed;

	// Resolved once from the game data
	static const AttributeIds& get(const GameData& gameData);
};

// AttributeModifier: added to the base value (ADD_VALUE), to a multiple of the base (ADD_MULTIPLIED_BASE) or
// multiplying the total (ADD_MULTIPLIED_TOTAL)
struct AttributeModifier {
	enum class Operation : uint8_t { AddValue = 0, AddMultipliedBase = 1, AddMultipliedTotal = 2 };
	std::string id; // "minecraft:random_spawn_bonus"
	double		amount	  = 0;
	Operation	operation = Operation::AddValue;
};

// AttributeInstance: an attribute's base value and modifiers, its value computed when it changes
class AttributeInstance {
  public:
	AttributeInstance(int attribute, double base) : _attribute(attribute), _base(base) {}

	int	   attribute() const { return _attribute; }
	double baseValue() const { return _base; }
	void   setBaseValue(double value);
	double value(const GameData::AttributeInfo& info) const;
	// addPermanentModifier / addTransientModifier: permanent ones are saved. False if the id is there already
	bool   addModifier(const AttributeModifier& modifier, bool permanent);
	bool   removeModifier(const std::string& id);
	bool   hasModifier(const std::string& id) const;
	const std::vector<AttributeModifier>& modifiers() const { return _modifiers; }
	bool								  isPermanent(size_t index) const { return _permanent[index]; }
	bool								  dirty = false; // Changed since sent to the client

  private:
	int							   _attribute;
	double						   _base;
	std::vector<AttributeModifier> _modifiers;
	std::vector<bool>			   _permanent;
	mutable double				   _cached		= 0;
	mutable bool				   _cacheStale	= true;
};

// AttributeMap: the attributes of a living entity. Like vanilla, an instance is only created the first time an
// attribute is changed (getInstance); until then values come straight from the type's defaults (its AttributeSupplier),
// so a mob costs nothing for the attributes it never changes
class AttributeMap {
  public:
	AttributeMap(const GameData& gameData, const GameData::EntityTypeInfo& type) : _gameData(gameData), _type(type) {}

	bool			   hasAttribute(int attribute) const;
	double			   getValue(int attribute) const;
	double			   getBaseValue(int attribute) const;
	// nullptr if the type doesn't have this attribute
	AttributeInstance* getInstance(int attribute);
	// getSyncableAttributes: the instances the client is told about when it starts seeing the entity
	template <typename F> void forEachSyncable(F f) const {
		for (const AttributeInstance& instance : _instances) {
			if (_gameData.getAttribute(instance.attribute()).syncable) f(instance);
		}
	}
	bool hasDirtySyncable() const;
	void clearDirty();

	// Update Attributes packet entries (count, then per attribute: id, base, modifiers), for `dirtyOnly` or all syncable
	void writeSyncable(Buffer& buf, bool dirtyOnly) const;
	// Saved: base values and permanent modifiers of the instances (vanilla's "attributes" list)
	void save(Buffer& buf) const;
	void load(Buffer& buf);

  private:
	const GameData&					_gameData;
	const GameData::EntityTypeInfo& _type;
	std::deque<AttributeInstance>	_instances; // Few: a linear search is the fastest. A deque keeps them in place
};

#endif
