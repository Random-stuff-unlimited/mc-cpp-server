#include "world/entity/MobEffects.hpp"

#include "data/GameData.hpp"
#include "lib/nbt.hpp"
#include "network/buffer.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"

#include <optional>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace MobEffects {
	namespace {
		using Op = AttributeModifier::Operation;
		constexpr const char* EFFECT_PARTICLE = "minecraft:entity_effect";
		constexpr int		  AMBIENT_ALPHA	  = 38; // Mth.floor(38.25F)

		// MobEffects, in registration order
		const std::array<Info, Count>& table() {
			static const std::array<Info, Count> effects = {{
					{"minecraft:speed", Category::Beneficial, 3402751, EFFECT_PARTICLE, false, {{"minecraft:movement_speed", "minecraft:effect.speed", 0.2F, Op::AddMultipliedTotal}}},
					{"minecraft:slowness", Category::Harmful, 9154528, EFFECT_PARTICLE, false,
					 {{"minecraft:movement_speed", "minecraft:effect.slowness", -0.15F, Op::AddMultipliedTotal}}},
					{"minecraft:haste", Category::Beneficial, 14270531, EFFECT_PARTICLE, false, {{"minecraft:attack_speed", "minecraft:effect.haste", 0.1F, Op::AddMultipliedTotal}}},
					{"minecraft:mining_fatigue", Category::Harmful, 4866583, EFFECT_PARTICLE, false,
					 {{"minecraft:attack_speed", "minecraft:effect.mining_fatigue", -0.1F, Op::AddMultipliedTotal}}},
					{"minecraft:strength", Category::Beneficial, 16762624, EFFECT_PARTICLE, false, {{"minecraft:attack_damage", "minecraft:effect.strength", 3.0, Op::AddValue}}},
					{"minecraft:instant_health", Category::Beneficial, 16262179, EFFECT_PARTICLE, true, {}},
					{"minecraft:instant_damage", Category::Harmful, 11101546, EFFECT_PARTICLE, true, {}},
					{"minecraft:jump_boost", Category::Beneficial, 16646020, EFFECT_PARTICLE, false,
					 {{"minecraft:safe_fall_distance", "minecraft:effect.jump_boost", 1.0, Op::AddValue}}},
					{"minecraft:nausea", Category::Harmful, 5578058, EFFECT_PARTICLE, false, {}},
					{"minecraft:regeneration", Category::Beneficial, 13458603, EFFECT_PARTICLE, false, {}},
					{"minecraft:resistance", Category::Beneficial, 9520880, EFFECT_PARTICLE, false, {}},
					{"minecraft:fire_resistance", Category::Beneficial, 16750848, EFFECT_PARTICLE, false, {}},
					{"minecraft:water_breathing", Category::Beneficial, 10017472, EFFECT_PARTICLE, false, {}},
					{"minecraft:invisibility", Category::Beneficial, 16185078, EFFECT_PARTICLE, false,
					 {{"minecraft:waypoint_transmit_range", "minecraft:effect.waypoint_transmit_range_hide", -1.0, Op::AddMultipliedTotal}}},
					{"minecraft:blindness", Category::Harmful, 2039587, EFFECT_PARTICLE, false, {}},
					{"minecraft:night_vision", Category::Beneficial, 12779366, EFFECT_PARTICLE, false, {}},
					{"minecraft:hunger", Category::Harmful, 5797459, EFFECT_PARTICLE, false, {}},
					{"minecraft:weakness", Category::Harmful, 4738376, EFFECT_PARTICLE, false, {{"minecraft:attack_damage", "minecraft:effect.weakness", -4.0, Op::AddValue}}},
					{"minecraft:poison", Category::Harmful, 8889187, EFFECT_PARTICLE, false, {}},
					{"minecraft:wither", Category::Harmful, 7561558, EFFECT_PARTICLE, false, {}},
					{"minecraft:health_boost", Category::Beneficial, 16284963, EFFECT_PARTICLE, false, {{"minecraft:max_health", "minecraft:effect.health_boost", 4.0, Op::AddValue}}},
					{"minecraft:absorption", Category::Beneficial, 2445989, EFFECT_PARTICLE, false, {{"minecraft:max_absorption", "minecraft:effect.absorption", 4.0, Op::AddValue}}},
					{"minecraft:saturation", Category::Beneficial, 16262179, EFFECT_PARTICLE, true, {}},
					{"minecraft:glowing", Category::Neutral, 9740385, EFFECT_PARTICLE, false, {}},
					{"minecraft:levitation", Category::Harmful, 13565951, EFFECT_PARTICLE, false, {}},
					{"minecraft:luck", Category::Beneficial, 5882118, EFFECT_PARTICLE, false, {{"minecraft:luck", "minecraft:effect.luck", 1.0, Op::AddValue}}},
					{"minecraft:unluck", Category::Harmful, 12624973, EFFECT_PARTICLE, false, {{"minecraft:luck", "minecraft:effect.unluck", -1.0, Op::AddValue}}},
					{"minecraft:slow_falling", Category::Beneficial, 15978425, EFFECT_PARTICLE, false, {}},
					{"minecraft:conduit_power", Category::Beneficial, 1950417, EFFECT_PARTICLE, false, {}},
					{"minecraft:dolphins_grace", Category::Beneficial, 8954814, EFFECT_PARTICLE, false, {}},
					{"minecraft:bad_omen", Category::Neutral, 745784, EFFECT_PARTICLE, false, {}},
					{"minecraft:hero_of_the_village", Category::Beneficial, 4521796, EFFECT_PARTICLE, false, {}},
					{"minecraft:darkness", Category::Harmful, 2696993, EFFECT_PARTICLE, false, {}},
					{"minecraft:trial_omen", Category::Neutral, 1484454, "minecraft:trial_omen", false, {}},
					{"minecraft:raid_omen", Category::Neutral, 14565464, "minecraft:raid_omen", false, {}},
					{"minecraft:wind_charged", Category::Harmful, 12438015, EFFECT_PARTICLE, false, {}},
					{"minecraft:weaving", Category::Harmful, 7891290, EFFECT_PARTICLE, false, {}},
					{"minecraft:oozing", Category::Harmful, 10092451, EFFECT_PARTICLE, false, {}},
					{"minecraft:infested", Category::Harmful, 9214860, EFFECT_PARTICLE, false, {}},
			}};
			return effects;
		}

		// MobEffect.withSoundOnAdded
		const char* soundOnAdded(Effect effect) {
			switch (effect) {
			case BadOmen: return "minecraft:event.mob_effect.bad_omen";
			case TrialOmen: return "minecraft:event.mob_effect.trial_omen";
			case RaidOmen: return "minecraft:event.mob_effect.raid_omen";
			default: return nullptr;
			}
		}

		// Java's int shifts: the distance is taken modulo 32, and a left shift wraps around
		int32_t shiftLeft(int32_t value, int distance) { return static_cast<int32_t>(static_cast<uint32_t>(value) << (distance & 31)); }
		int32_t shiftRight(int32_t value, int distance) { return value >> (distance & 31); }

		// HealOrHarmMobEffect.applyEffectTick
		void healOrHarm(Host& host, bool isHarm, int amplifier, int heal, int harm, const Combat::DamageSource& source) {
			(void)amplifier;
			bool inverted = host.effectGameData().isInTag("minecraft:entity_type", "minecraft:inverted_healing_and_harm", host.effectTypeId());
			if (isHarm == inverted) {
				host.effectHeal(static_cast<float>(std::max(heal, 0)));
			} else {
				host.effectHurt(source, static_cast<float>(harm));
			}
		}

		void writeInstance(Buffer& buf, const Instance& instance) {
			buf.writeString(info(instance.effect).name);
			buf.writeVarInt(instance.duration);
			buf.writeVarInt(instance.amplifier);
			buf.writeUByte(static_cast<uint8_t>(instance.ambient | instance.visible << 1 | instance.showIcon << 2));
			buf.writeBool(instance.hidden != nullptr);
			if (instance.hidden) writeInstance(buf, *instance.hidden);
		}

		// Unknown effects are read and dropped (nullopt)
		std::optional<Instance> readInstance(Buffer& buf) {
			Effect	effect	  = byName(buf.readString());
			int		duration  = buf.readVarInt();
			int		amplifier = buf.readVarInt();
			uint8_t flags	  = buf.readUByte();
			Instance instance(effect == Count ? Speed : effect, duration, amplifier, flags & 1, flags & 2, flags & 4);
			if (buf.readBool()) {
				if (std::optional<Instance> hidden = readInstance(buf)) instance.hidden = std::make_unique<Instance>(std::move(*hidden));
			}
			if (effect == Count) return std::nullopt;
			return instance;
		}

		// MobEffectInstance.Details.MAP_CODEC, the fields of an "active_effects" entry (without its id)
		void readDetails(const nbt::TagCompound& tag, Instance& instance) {
			auto number = [&](const char* key, int64_t fallback) -> int64_t {
				if (!tag.contains(key)) return fallback;
				return std::visit(
						[&](const auto& value) -> int64_t {
							using T = std::decay_t<decltype(value)>;
							if constexpr (std::is_integral_v<T>) {
								return static_cast<int64_t>(value);
							} else {
								return fallback;
							}
						},
						tag.at(key).data);
			};
			instance.amplifier = static_cast<int>(std::clamp<int64_t>(number("amplifier", 0) & 0xFF, 0, 255)); // UNSIGNED_BYTE
			instance.duration  = static_cast<int>(number("duration", 0));
			instance.ambient   = number("ambient", 0) != 0;
			instance.visible   = number("show_particles", 1) != 0;
			instance.showIcon  = number("show_icon", instance.visible) != 0;
			if (tag.contains("hidden_effect")) {
				if (auto* hidden = std::get_if<std::shared_ptr<nbt::TagCompound>>(&tag.at("hidden_effect").data); hidden && *hidden) {
					instance.hidden			= std::make_unique<Instance>();
					instance.hidden->effect = instance.effect;
					readDetails(**hidden, *instance.hidden);
				}
			}
		}

		nbt::TagCompound writeDetails(const Instance& instance) {
			nbt::TagCompound tag;
			tag["amplifier"]	  = static_cast<nbt::TagByte>(instance.amplifier);
			tag["duration"]		  = static_cast<nbt::TagInt>(instance.duration);
			tag["ambient"]		  = static_cast<nbt::TagByte>(instance.ambient);
			tag["show_particles"] = static_cast<nbt::TagByte>(instance.visible);
			tag["show_icon"]	  = static_cast<nbt::TagByte>(instance.showIcon);
			if (instance.hidden) tag["hidden_effect"] = std::make_shared<nbt::TagCompound>(writeDetails(*instance.hidden));
			return tag;
		}
	} // namespace

	const Info& info(Effect effect) { return table()[effect]; }

	Effect byName(const std::string& name) {
		std::string full = name.find(':') == std::string::npos ? "minecraft:" + name : name;
		for (int i = 0; i < Count; i++) {
			if (full == table()[i].name) return static_cast<Effect>(i);
		}
		return Count;
	}

	void verify(const GameData& gameData) {
		for (int i = 0; i < Count; i++) {
			if (gameData.getStaticId("minecraft:mob_effect", table()[i].name) != i) throw std::runtime_error(std::string("mob effect out of order: ") + table()[i].name);
		}
	}

	std::vector<Instance> potionEffects(const std::string& potion) {
		std::string name = potion.find(':') == std::string::npos ? potion : potion.substr(potion.find(':') + 1);
		struct Entry {
			const char*			  name;
			std::vector<Instance> effects;
		};
		// Potions
		static const std::vector<Entry> potions = {
				{"night_vision", {{NightVision, 3600}}},
				{"long_night_vision", {{NightVision, 9600}}},
				{"invisibility", {{Invisibility, 3600}}},
				{"long_invisibility", {{Invisibility, 9600}}},
				{"leaping", {{JumpBoost, 3600}}},
				{"long_leaping", {{JumpBoost, 9600}}},
				{"strong_leaping", {{JumpBoost, 1800, 1}}},
				{"fire_resistance", {{FireResistance, 3600}}},
				{"long_fire_resistance", {{FireResistance, 9600}}},
				{"swiftness", {{Speed, 3600}}},
				{"long_swiftness", {{Speed, 9600}}},
				{"strong_swiftness", {{Speed, 1800, 1}}},
				{"slowness", {{Slowness, 1800}}},
				{"long_slowness", {{Slowness, 4800}}},
				{"strong_slowness", {{Slowness, 400, 3}}},
				{"turtle_master", {{Slowness, 400, 3}, {Resistance, 400, 2}}},
				{"long_turtle_master", {{Slowness, 800, 3}, {Resistance, 800, 2}}},
				{"strong_turtle_master", {{Slowness, 400, 5}, {Resistance, 400, 3}}},
				{"water_breathing", {{WaterBreathing, 3600}}},
				{"long_water_breathing", {{WaterBreathing, 9600}}},
				{"healing", {{InstantHealth, 1}}},
				{"strong_healing", {{InstantHealth, 1, 1}}},
				{"harming", {{InstantDamage, 1}}},
				{"strong_harming", {{InstantDamage, 1, 1}}},
				{"poison", {{Poison, 900}}},
				{"long_poison", {{Poison, 1800}}},
				{"strong_poison", {{Poison, 432, 1}}},
				{"regeneration", {{Regeneration, 900}}},
				{"long_regeneration", {{Regeneration, 1800}}},
				{"strong_regeneration", {{Regeneration, 450, 1}}},
				{"strength", {{Strength, 3600}}},
				{"long_strength", {{Strength, 9600}}},
				{"strong_strength", {{Strength, 1800, 1}}},
				{"weakness", {{Weakness, 1800}}},
				{"long_weakness", {{Weakness, 4800}}},
				{"luck", {{Luck, 6000}}},
				{"slow_falling", {{SlowFalling, 1800}}},
				{"long_slow_falling", {{SlowFalling, 4800}}},
				{"wind_charged", {{WindCharged, 3600}}},
				{"weaving", {{Weaving, 3600}}},
				{"oozing", {{Oozing, 3600}}},
				{"infested", {{Infested, 3600}}},
		};
		for (const Entry& entry : potions) {
			if (name == entry.name) return entry.effects;
		}
		return {};
	}

	void applyPotion(Host& host, Active& effects, const std::string& potion, float durationScale, Actor* source) {
		// PotionContents.forEachEffect (withScaledDuration), then applyToLivingEntity
		for (Instance instance : potionEffects(potion)) {
			instance.scaleDuration(durationScale);
			if (info(instance.effect).instantaneous) {
				applyInstantaneous(host, instance, 1.0, source, source);
			} else {
				effects.add(host, instance, source != nullptr);
			}
		}
	}

	bool shouldApplyEffectTickThisTick(Effect effect, int tick, int amplifier) {
		switch (effect) {
		case InstantHealth:
		case InstantDamage:
		case Saturation: return tick >= 1; // InstantenousMobEffect
		case Absorption:
		case Hunger:
		case BadOmen: return true;
		case Regeneration: {
			int interval = shiftRight(50, amplifier);
			return interval > 0 ? tick % interval == 0 : true;
		}
		case Poison: {
			int interval = shiftRight(25, amplifier);
			return interval > 0 ? tick % interval == 0 : true;
		}
		case Wither: {
			int interval = shiftRight(40, amplifier);
			return interval > 0 ? tick % interval == 0 : true;
		}
		default: return false;
		}
	}

	void applyInstantaneous(Host& host, const Instance& instance, double effectiveness, Actor* source, Actor* direct) {
		if (instance.effect == InstantHealth || instance.effect == InstantDamage) {
			// HealOrHarmMobEffect.applyInstantenousEffect
			int heal = static_cast<int>(effectiveness * shiftLeft(4, instance.amplifier) + 0.5);
			int harm = static_cast<int>(effectiveness * shiftLeft(6, instance.amplifier) + 0.5);
			Combat::DamageSource damage{"minecraft:magic", nullptr};
			if (source) damage = {"minecraft:indirect_magic", source, direct};
			healOrHarm(host, instance.effect == InstantDamage, instance.amplifier, heal, harm, damage);
			return;
		}
		// MobEffect.applyInstantenousEffect: one applyEffectTick
		switch (instance.effect) {
		case Saturation: host.effectEat(instance.amplifier + 1, 1.0F); break;
		default: break;
		}
	}

	// ----- Instance -----

	Instance::Instance(const Instance& other) { *this = other; }

	Instance& Instance::operator=(const Instance& other) {
		if (this == &other) return *this;
		effect	  = other.effect;
		duration  = other.duration;
		amplifier = other.amplifier;
		ambient	  = other.ambient;
		visible	  = other.visible;
		showIcon  = other.showIcon;
		hidden	  = other.hidden ? std::make_unique<Instance>(*other.hidden) : nullptr;
		return *this;
	}

	void Instance::setDetailsFrom(const Instance& other) {
		duration  = other.duration;
		amplifier = other.amplifier;
		ambient	  = other.ambient;
		visible	  = other.visible;
		showIcon  = other.showIcon;
	}

	bool Instance::isShorterDurationThan(const Instance& other) const {
		return !isInfiniteDuration() && (duration < other.duration || other.isInfiniteDuration());
	}

	bool Instance::update(const Instance& other) {
		bool changed = false;
		if (other.amplifier > amplifier) {
			if (other.isShorterDurationThan(*this)) {
				// The current one waits behind the stronger one
				std::unique_ptr<Instance> previous = std::move(hidden);
				hidden								= std::make_unique<Instance>(*this);
				hidden->hidden						= std::move(previous);
			}
			amplifier = other.amplifier;
			duration  = other.duration;
			changed	  = true;
		} else if (isShorterDurationThan(other)) {
			if (other.amplifier == amplifier) {
				duration = other.duration;
				changed	 = true;
			} else if (!hidden) {
				hidden = std::make_unique<Instance>(other);
			} else {
				hidden->update(other);
			}
		}
		if ((!other.ambient && ambient) || changed) {
			ambient = other.ambient;
			changed = true;
		}
		if (other.visible != visible) {
			visible = other.visible;
			changed = true;
		}
		if (other.showIcon != showIcon) {
			showIcon = other.showIcon;
			changed	 = true;
		}
		return changed;
	}

	void Instance::scaleDuration(float factor) {
		if (!isInfiniteDuration() && duration != 0) duration = std::max(static_cast<int>(std::floor(duration * factor)), 1);
	}

	uint8_t Instance::packetFlags(bool blend) const {
		return static_cast<uint8_t>((ambient ? 1 : 0) | (visible ? 2 : 0) | (showIcon ? 4 : 0) | (blend ? 8 : 0));
	}

	void Instance::tickDownDuration() {
		if (hidden) hidden->tickDownDuration();
		if (!isInfiniteDuration() && duration != 0) duration--;
	}

	bool Instance::downgradeToHiddenEffect() {
		if (duration != 0 || !hidden) return false;
		setDetailsFrom(*hidden);
		std::unique_ptr<Instance> next = std::move(hidden->hidden);
		hidden						   = std::move(next);
		return true;
	}

	// ----- Active -----

	const Instance* Active::get(Effect effect) const {
		for (const Instance& i : _effects) {
			if (i.effect == effect) return &i;
		}
		return nullptr;
	}

	Instance* Active::get(Effect effect) { return const_cast<Instance*>(static_cast<const Active*>(this)->get(effect)); }

	bool Active::canBeAffected(const GameData& gameData, int typeId, Effect effect) {
		if (gameData.isInTag("minecraft:entity_type", "minecraft:immune_to_infested", typeId)) return effect != Infested;
		if (gameData.isInTag("minecraft:entity_type", "minecraft:immune_to_oozing", typeId)) return effect != Oozing;
		if (!gameData.isInTag("minecraft:entity_type", "minecraft:ignores_poison_and_regen", typeId)) return true;
		return effect != Regeneration && effect != Poison;
	}

	void Active::applyModifiers(Host& host, const Instance& instance) {
		AttributeMap* attributes = host.effectAttributes();
		if (!attributes) return;
		const GameData& data = host.effectGameData();
		for (const AttributeTemplate& t : info(instance.effect).modifiers) {
			AttributeInstance* attribute = attributes->getInstance(data.getStaticId("minecraft:attribute", t.attribute));
			if (!attribute) continue;
			attribute->removeModifier(t.id);
			attribute->addModifier({t.id, t.amount * (instance.amplifier + 1), t.operation}, true);
		}
	}

	void Active::removeModifiers(Host& host, Effect effect) {
		AttributeMap* attributes = host.effectAttributes();
		if (!attributes) return;
		const GameData& data = host.effectGameData();
		for (const AttributeTemplate& t : info(effect).modifiers) {
			if (AttributeInstance* attribute = attributes->getInstance(data.getStaticId("minecraft:attribute", t.attribute))) attribute->removeModifier(t.id);
		}
	}

	void Active::onAdded(Host& host, const Instance& instance) {
		visualsDirty = true;
		applyModifiers(host, instance);
		host.sendEffectUpdate(instance, true);
	}

	void Active::onUpdated(Host& host, const Instance& instance, bool reapply) {
		visualsDirty = true;
		if (reapply) {
			removeModifiers(host, instance.effect);
			applyModifiers(host, instance);
			host.onEffectAttributesChanged();
		}
		host.sendEffectUpdate(instance, false);
	}

	void Active::onRemoved(Host& host, const Instance& instance) {
		visualsDirty = true;
		removeModifiers(host, instance.effect);
		host.sendEffectRemoved(instance.effect);
		host.onEffectAttributesChanged();
	}

	void Active::onStarted(Host& host, const Instance& instance) {
		// AbsorptionMobEffect.onEffectStarted
		if (instance.effect == Absorption) host.setEffectAbsorption(std::max(host.effectAbsorption(), static_cast<float>(4 * (1 + instance.amplifier))));
	}

	bool Active::add(Host& host, const Instance& instance, bool) {
		if (!host.effectCanBeAffected(instance.effect)) return false;
		Instance* existing = get(instance.effect);
		bool	  changed  = false;
		if (!existing) {
			auto at = std::lower_bound(_effects.begin(), _effects.end(), instance.effect, [](const Instance& i, Effect e) { return i.effect < e; });
			const Instance& added = *_effects.insert(at, instance);
			onAdded(host, added);
			changed = true;
			if (const char* sound = soundOnAdded(instance.effect)) host.playEffectSound(sound); // MobEffect.onEffectAdded
		} else if (existing->update(instance)) {
			onUpdated(host, *existing, true);
			changed = true;
		}
		onStarted(host, instance);
		return changed;
	}

	void Active::forceAdd(Host& host, const Instance& instance) {
		if (!host.effectCanBeAffected(instance.effect)) return;
		if (Instance* existing = get(instance.effect)) {
			*existing = instance;
			onUpdated(host, *existing, true);
		} else {
			auto at = std::lower_bound(_effects.begin(), _effects.end(), instance.effect, [](const Instance& i, Effect e) { return i.effect < e; });
			onAdded(host, *_effects.insert(at, instance));
		}
	}

	bool Active::remove(Host& host, Effect effect) {
		for (auto it = _effects.begin(); it != _effects.end(); ++it) {
			if (it->effect != effect) continue;
			Instance removed = std::move(*it);
			_effects.erase(it);
			onRemoved(host, removed);
			return true;
		}
		return false;
	}

	bool Active::removeAll(Host& host) {
		if (_effects.empty()) return false;
		std::vector<Instance> removed = std::move(_effects);
		_effects.clear();
		for (const Instance& instance : removed) onRemoved(host, instance);
		return true;
	}

	bool Active::applyTick(Host& host, const Instance& instance) {
		int amplifier = instance.amplifier;
		switch (instance.effect) {
		case Regeneration:
			if (host.effectHealth() < host.effectMaxHealth()) host.effectHeal(1.0F);
			return true;
		case Poison:
			if (host.effectHealth() > 1.0F) host.effectHurt({"minecraft:magic", nullptr}, 1.0F);
			return true;
		case Wither: host.effectHurt({"minecraft:wither", nullptr}, 1.0F); return true;
		case Hunger: host.effectFoodExhaustion(0.005F * (amplifier + 1)); return true;
		case Saturation: host.effectEat(amplifier + 1, 1.0F); return true;
		case InstantHealth:
		case InstantDamage:
			healOrHarm(host, instance.effect == InstantDamage, amplifier, shiftLeft(4, amplifier), shiftLeft(6, amplifier), {"minecraft:magic", nullptr});
			return true;
		case Absorption: return host.effectAbsorption() > 0.0F;
		default: return true; // BadOmen: raids aren't ported
		}
	}

	void Active::tick(Host& host) {
		if (!_effects.empty()) {
			// By effect: applying one may hurt the host, which may change the list
			Effect order[Count];
			size_t count = 0;
			for (const Instance& i : _effects) order[count++] = i.effect;
			for (size_t n = 0; n < count; n++) {
				Instance* instance = get(order[n]);
				if (!instance) continue;
				// MobEffectInstance.tickServer
				bool keep = instance->isInfiniteDuration() || instance->duration > 0;
				if (keep) {
					int tick = instance->isInfiniteDuration() ? static_cast<int>(host.effectTickCount()) : instance->duration;
					if (shouldApplyEffectTickThisTick(instance->effect, tick, instance->amplifier)) {
						Instance copy = *instance;
						keep		  = applyTick(host, copy);
						instance	  = get(order[n]);
						if (!instance) continue;
					}
					if (keep) {
						instance->tickDownDuration();
						if (instance->downgradeToHiddenEffect()) onUpdated(host, *instance, true);
						keep = instance->isInfiniteDuration() || instance->duration > 0;
					}
				}
				if (!keep) {
					remove(host, order[n]);
				} else if (instance->duration % 600 == 0) {
					onUpdated(host, *instance, false);
				}
			}
		}
		if (visualsDirty) {
			host.updateEffectVisuals();
			visualsDirty = false;
		}
	}

	void Active::reapplyModifiers(Host& host) {
		for (const Instance& instance : _effects) applyModifiers(host, instance);
	}

	bool Active::allAmbient() const {
		for (const Instance& i : _effects) {
			if (i.visible && !i.ambient) return false;
		}
		return true;
	}

	bool Active::writeParticles(Buffer& buf, const GameData& gameData) const {
		int count = 0;
		for (const Instance& i : _effects) count += i.visible;
		buf.writeVarInt(count);
		for (const Instance& i : _effects) {
			if (!i.visible) continue;
			const Info& effect = info(i.effect);
			buf.writeVarInt(gameData.getStaticId("minecraft:particle_type", effect.particle));
			if (effect.particle == std::string(EFFECT_PARTICLE)) {
				// ColorParticleOption: ARGB, faint for ambient effects (beacons)
				int alpha = i.ambient ? AMBIENT_ALPHA : 255;
				buf.writeInt(static_cast<int32_t>(static_cast<uint32_t>(alpha) << 24 | (static_cast<uint32_t>(effect.color) & 0xFFFFFF)));
			}
		}
		return count > 0;
	}

	void Active::save(Buffer& buf) const {
		buf.writeVarInt(static_cast<int32_t>(_effects.size()));
		for (const Instance& instance : _effects) writeInstance(buf, instance);
	}

	void Active::load(Buffer& buf) {
		_effects.clear();
		int count = buf.readVarInt();
		for (int i = 0; i < count; i++) {
			std::optional<Instance> instance = readInstance(buf);
			if (!instance || get(instance->effect)) continue;
			auto at = std::lower_bound(_effects.begin(), _effects.end(), instance->effect, [](const Instance& e, Effect id) { return e.effect < id; });
			_effects.insert(at, std::move(*instance));
		}
		visualsDirty = true;
	}

	void Active::loadNbt(const nbt::TagList& list) {
		_effects.clear();
		for (size_t n = 0; n < list.size(); n++) {
			auto* entry = std::get_if<std::shared_ptr<nbt::TagCompound>>(&list[n].data);
			if (!entry || !*entry || !(*entry)->contains("id")) continue;
			auto* id = std::get_if<std::string>(&(*entry)->at("id").data);
			if (!id) continue;
			Effect effect = byName(*id);
			if (effect == Count || get(effect)) continue;
			Instance instance;
			instance.effect = effect;
			readDetails(**entry, instance);
			auto at = std::lower_bound(_effects.begin(), _effects.end(), effect, [](const Instance& e, Effect i) { return e.effect < i; });
			_effects.insert(at, std::move(instance));
		}
		visualsDirty = true;
	}

	void Active::saveNbt(nbt::TagList& list) const {
		for (const Instance& instance : _effects) {
			nbt::TagCompound tag = writeDetails(instance);
			tag["id"]			 = std::string(info(instance.effect).name);
			list.push_back(nbt::Tag(std::make_shared<nbt::TagCompound>(std::move(tag))));
		}
	}
} // namespace MobEffects
