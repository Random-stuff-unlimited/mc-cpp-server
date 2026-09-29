#include "world/entity/Attributes.hpp"

#include "network/buffer.hpp"

#include <algorithm>
#include <cmath>

const AttributeIds& AttributeIds::get(const GameData& gameData) {
	static const AttributeIds ids = [&] {
		auto		 id = [&](const char* name) { return gameData.getStaticId("minecraft:attribute", name); };
		AttributeIds a{};
		a.armor					  = id("minecraft:armor");
		a.armorToughness		  = id("minecraft:armor_toughness");
		a.attackDamage			  = id("minecraft:attack_damage");
		a.attackKnockback		  = id("minecraft:attack_knockback");
		a.fallDamageMultiplier	  = id("minecraft:fall_damage_multiplier");
		a.followRange			  = id("minecraft:follow_range");
		a.gravity				  = id("minecraft:gravity");
		a.jumpStrength			  = id("minecraft:jump_strength");
		a.knockbackResistance	  = id("minecraft:knockback_resistance");
		a.maxHealth				  = id("minecraft:max_health");
		a.movementEfficiency	  = id("minecraft:movement_efficiency");
		a.movementSpeed			  = id("minecraft:movement_speed");
		a.oxygenBonus			  = id("minecraft:oxygen_bonus");
		a.safeFallDistance		  = id("minecraft:safe_fall_distance");
		a.scale					  = id("minecraft:scale");
		a.stepHeight			  = id("minecraft:step_height");
		a.waterMovementEfficiency = id("minecraft:water_movement_efficiency");
		a.explosionKnockbackResistance = id("minecraft:explosion_knockback_resistance");
		a.attackSpeed				   = id("minecraft:attack_speed");
		a.flyingSpeed				   = id("minecraft:flying_speed");
		a.luck						   = id("minecraft:luck");
		a.spawnReinforcements		   = id("minecraft:spawn_reinforcements");
		a.temptRange				   = id("minecraft:tempt_range");
		a.burningTime				   = id("minecraft:burning_time");
		a.sweepingDamageRatio		   = id("minecraft:sweeping_damage_ratio");
		a.miningEfficiency			   = id("minecraft:mining_efficiency");
		a.blockBreakSpeed			   = id("minecraft:block_break_speed");
		a.submergedMiningSpeed		   = id("minecraft:submerged_mining_speed");
		a.sneakingSpeed				   = id("minecraft:sneaking_speed");
		return a;
	}();
	return ids;
}

// ----- AttributeInstance -----

void AttributeInstance::setBaseValue(double value) {
	if (value == _base) return;
	_base		= value;
	_cacheStale = true;
	dirty		= true;
}

bool AttributeInstance::addModifier(const AttributeModifier& modifier, bool permanent) {
	if (hasModifier(modifier.id)) return false; // Vanilla throws: already applied
	_modifiers.push_back(modifier);
	_permanent.push_back(permanent);
	_cacheStale = true;
	dirty		= true;
	return true;
}

bool AttributeInstance::removeModifier(const std::string& id) {
	for (size_t i = 0; i < _modifiers.size(); i++) {
		if (_modifiers[i].id != id) continue;
		_modifiers.erase(_modifiers.begin() + static_cast<long>(i));
		_permanent.erase(_permanent.begin() + static_cast<long>(i));
		_cacheStale = true;
		dirty		= true;
		return true;
	}
	return false;
}

bool AttributeInstance::hasModifier(const std::string& id) const {
	return std::any_of(_modifiers.begin(), _modifiers.end(), [&](const AttributeModifier& m) { return m.id == id; });
}

// AttributeInstance.calculateValue: additions, then multiples of that sum, then total multipliers; clamped to the range
double AttributeInstance::value(const GameData::AttributeInfo& info) const {
	if (!_cacheStale) return _cached;
	double added = _base;
	for (const AttributeModifier& m : _modifiers) {
		if (m.operation == AttributeModifier::Operation::AddValue) added += m.amount;
	}
	double total = added;
	for (const AttributeModifier& m : _modifiers) {
		if (m.operation == AttributeModifier::Operation::AddMultipliedBase) total += added * m.amount;
	}
	for (const AttributeModifier& m : _modifiers) {
		if (m.operation == AttributeModifier::Operation::AddMultipliedTotal) total *= 1.0 + m.amount;
	}
	_cached		= std::isnan(total) ? info.minValue : std::clamp(total, info.minValue, info.maxValue); // RangedAttribute.sanitizeValue
	_cacheStale = false;
	return _cached;
}

// ----- AttributeMap -----

bool AttributeMap::hasAttribute(int attribute) const {
	return attribute >= 0 && static_cast<size_t>(attribute) < _type.attributes.size() && !std::isnan(_type.attributes[attribute]);
}

double AttributeMap::getValue(int attribute) const {
	for (const AttributeInstance& instance : _instances) {
		if (instance.attribute() == attribute) return instance.value(_gameData.getAttribute(attribute));
	}
	const GameData::AttributeInfo& info = _gameData.getAttribute(attribute);
	double						   base = hasAttribute(attribute) ? _type.attributes[attribute] : info.defaultValue;
	return std::clamp(base, info.minValue, info.maxValue);
}

double AttributeMap::getBaseValue(int attribute) const {
	for (const AttributeInstance& instance : _instances) {
		if (instance.attribute() == attribute) return instance.baseValue();
	}
	return hasAttribute(attribute) ? _type.attributes[attribute] : _gameData.getAttribute(attribute).defaultValue;
}

AttributeInstance* AttributeMap::getInstance(int attribute) {
	for (AttributeInstance& instance : _instances) {
		if (instance.attribute() == attribute) return &instance;
	}
	if (!hasAttribute(attribute)) return nullptr;
	return &_instances.emplace_back(attribute, _type.attributes[attribute]);
}

bool AttributeMap::hasDirtySyncable() const {
	return std::any_of(_instances.begin(), _instances.end(),
					   [&](const AttributeInstance& i) { return i.dirty && _gameData.getAttribute(i.attribute()).syncable; });
}

void AttributeMap::clearDirty() {
	for (AttributeInstance& instance : _instances) instance.dirty = false;
}

void AttributeMap::writeSyncable(Buffer& buf, bool dirtyOnly) const {
	int count = 0;
	forEachSyncable([&](const AttributeInstance& i) { count += !dirtyOnly || i.dirty; });
	buf.writeVarInt(count);
	forEachSyncable([&](const AttributeInstance& i) {
		if (dirtyOnly && !i.dirty) return;
		buf.writeVarInt(i.attribute());
		buf.writeDouble(i.baseValue());
		buf.writeVarInt(static_cast<int32_t>(i.modifiers().size()));
		for (const AttributeModifier& m : i.modifiers()) {
			buf.writeString(m.id);
			buf.writeDouble(m.amount);
			buf.writeByte(static_cast<int8_t>(m.operation));
		}
	});
}

void AttributeMap::save(Buffer& buf) const {
	buf.writeVarInt(static_cast<int32_t>(_instances.size()));
	for (const AttributeInstance& i : _instances) {
		buf.writeString(_gameData.getStaticName("minecraft:attribute", i.attribute()));
		buf.writeDouble(i.baseValue());
		int permanent = 0;
		for (size_t m = 0; m < i.modifiers().size(); m++) permanent += i.isPermanent(m);
		buf.writeVarInt(permanent);
		for (size_t m = 0; m < i.modifiers().size(); m++) {
			if (!i.isPermanent(m)) continue;
			buf.writeString(i.modifiers()[m].id);
			buf.writeDouble(i.modifiers()[m].amount);
			buf.writeByte(static_cast<int8_t>(i.modifiers()[m].operation));
		}
	}
}

void AttributeMap::load(Buffer& buf) {
	int count = buf.readVarInt();
	for (int n = 0; n < count; n++) {
		int				   attribute = _gameData.getStaticId("minecraft:attribute", buf.readString());
		double			   base		 = buf.readDouble();
		AttributeInstance* instance	 = getInstance(attribute); // Attributes this type doesn't have anymore are dropped
		if (instance) instance->setBaseValue(base);
		int modifiers = buf.readVarInt();
		for (int m = 0; m < modifiers; m++) {
			AttributeModifier modifier;
			modifier.id		   = buf.readString();
			modifier.amount	   = buf.readDouble();
			modifier.operation = static_cast<AttributeModifier::Operation>(buf.readByte());
			if (instance) instance->addModifier(modifier, true);
		}
		if (instance) instance->dirty = false;
	}
}
