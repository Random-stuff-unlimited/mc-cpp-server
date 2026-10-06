#ifndef MOB_EFFECTS_HPP
#define MOB_EFFECTS_HPP

#include "world/entity/Attributes.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class Actor;
class Buffer;
class GameData;
class Level;
namespace Combat {
	struct DamageSource;
}
namespace nbt {
	struct TagList;
}

// Status effects (vanilla's MobEffect, MobEffects, MobEffectInstance and the active effects of LivingEntity), shared
// by the mobs (LivingEntity) and the players, which each implement MobEffects::Host.
namespace MobEffects {
	// The minecraft:mob_effect registry, in its order (the ids are these values)
	enum Effect : uint8_t {
		Speed,
		Slowness,
		Haste,
		MiningFatigue,
		Strength,
		InstantHealth,
		InstantDamage,
		JumpBoost,
		Nausea,
		Regeneration,
		Resistance,
		FireResistance,
		WaterBreathing,
		Invisibility,
		Blindness,
		NightVision,
		Hunger,
		Weakness,
		Poison,
		Wither,
		HealthBoost,
		Absorption,
		Saturation,
		Glowing,
		Levitation,
		Luck,
		Unluck,
		SlowFalling,
		ConduitPower,
		DolphinsGrace,
		BadOmen,
		HeroOfTheVillage,
		Darkness,
		TrialOmen,
		RaidOmen,
		WindCharged,
		Weaving,
		Oozing,
		Infested,
		Count
	};
	enum class Category : uint8_t { Beneficial, Harmful, Neutral };

	// MobEffect.AttributeTemplate: a modifier of amount * (amplifier + 1)
	struct AttributeTemplate {
		const char*					 attribute; // "minecraft:movement_speed"
		const char*					 id;		// "minecraft:effect.speed"
		double						 amount;
		AttributeModifier::Operation operation;
	};
	struct Info {
		const char*					  name; // "minecraft:speed"
		Category					  category;
		int							  color;
		const char*					  particle; // "minecraft:entity_effect" (a color option), or a plain particle
		bool						  instantaneous;
		std::vector<AttributeTemplate> modifiers;
	};
	const Info& info(Effect effect);
	// The effect of a registry name ("minecraft:poison" or "poison"), Count if none
	Effect		byName(const std::string& name);
	// Checks that the table follows the game data's registry (at startup)
	void		verify(const GameData& gameData);

	constexpr int INFINITE_DURATION = -1;

	// MobEffectInstance: an effect for a duration at an amplifier, with a weaker longer one hidden behind a stronger
	// shorter one (it comes back when the stronger ends)
	struct Instance {
		Effect					  effect	= Speed;
		int						  duration	= 0;
		int						  amplifier = 0;
		bool					  ambient = false, visible = true, showIcon = true;
		std::unique_ptr<Instance> hidden;

		Instance() = default;
		Instance(Effect e, int d, int a = 0, bool amb = false, bool vis = true) : Instance(e, d, a, amb, vis, vis) {}
		Instance(Effect e, int d, int a, bool amb, bool vis, bool icon)
			: effect(e), duration(d), amplifier(a < 0 ? 0 : a > 255 ? 255 : a), ambient(amb), visible(vis), showIcon(icon) {}
		Instance(const Instance& other);
		Instance& operator=(const Instance& other);

		bool isInfiniteDuration() const { return duration == INFINITE_DURATION; }
		bool endsWithin(int ticks) const { return !isInfiniteDuration() && duration <= ticks; }
		// MobEffectInstance.update: merged with another of the same effect, true if something changed
		bool update(const Instance& other);
		// MobEffectInstance.withScaledDuration / mapDuration
		void scaleDuration(float factor);
		// The ClientboundUpdateMobEffectPacket flags (1 ambient, 2 visible, 4 icon, 8 blend)
		uint8_t packetFlags(bool blend) const;

	  private:
		friend class Active;
		bool isShorterDurationThan(const Instance& other) const;
		void setDetailsFrom(const Instance& other);
		void tickDownDuration();
		bool downgradeToHiddenEffect();
	};

	// What an effect acts on: a mob or a player
	class Host {
	  public:
		virtual ~Host() = default;
		virtual Level&		  effectLevel()						   = 0;
		virtual const GameData& effectGameData() const		   = 0;
		virtual int			  effectEntityId() const			   = 0;
		virtual int			  effectTypeId() const				   = 0; // minecraft:entity_type
		virtual int64_t		  effectTickCount() const			   = 0;
		virtual float		  effectHealth() const				   = 0;
		virtual float		  effectMaxHealth() const			   = 0;
		virtual void		  effectHeal(float amount)			   = 0;
		virtual bool		  effectHurt(const Combat::DamageSource& source, float amount) = 0;
		virtual AttributeMap* effectAttributes()				   = 0;
		// LivingEntity.canBeAffected (spiders ignore poison, wither skeletons wither...)
		virtual bool		  effectCanBeAffected(Effect effect) const = 0;
		// LivingEntity.refreshDirtyAttributes: health and absorption within their new maximum
		virtual void		  onEffectAttributesChanged() {}
		virtual float		  effectAbsorption() const			   = 0;
		virtual void		  setEffectAbsorption(float amount)	   = 0;
		// Player.causeFoodExhaustion and FoodData.eat, players only
		virtual void		  effectFoodExhaustion(float) {}
		virtual void		  effectEat(int, float) {}
		// ServerPlayer: the client is told about its own effects
		virtual void		  sendEffectUpdate(const Instance&, bool) {}
		virtual void		  sendEffectRemoved(Effect) {}
		// The particles, invisibility and glowing follow (entity data), after the effects changed
		virtual void		  updateEffectVisuals() = 0;
		// A sound at the host (MobEffect.withSoundOnAdded)
		virtual void		  playEffectSound(const std::string& sound) = 0;
	};

	// LivingEntity's active effects. Kept in effect id order (vanilla's HashMap order is its holders' identity hashes:
	// arbitrary)
	class Active {
	  public:
		bool			empty() const { return _effects.empty(); }
		bool			has(Effect effect) const { return get(effect) != nullptr; }
		const Instance* get(Effect effect) const;
		Instance*		get(Effect effect);
		// The amplifier of an effect, -1 without it
		int				amplifier(Effect effect) const {
			const Instance* i = get(effect);
			return i ? i->amplifier : -1;
		}
		const std::vector<Instance>& all() const { return _effects; }

		// LivingEntity.addEffect: false if it can't have it (canBeAffected) or nothing changed
		bool add(Host& host, const Instance& instance, bool sourceIsEntity = false);
		// LivingEntity.forceAddEffect
		void forceAdd(Host& host, const Instance& instance);
		bool remove(Host& host, Effect effect);
		// LivingEntity.removeAllEffects (milk)
		bool removeAll(Host& host);
		// LivingEntity.tickEffects (server side)
		void tick(Host& host);
		// Dropped without the removal logic (death: triggerOnDeathMobEffects clears them)
		void clear() { _effects.clear(); }
		// LivingEntity.canBeAffected, from the type's tags
		static bool canBeAffected(const GameData& gameData, int typeId, Effect effect);

		// The effects' attribute modifiers again (a loaded player, whose attributes aren't saved here)
		void reapplyModifiers(Host& host);
		// updateSynchronizedMobEffectParticles: the particles of the visible effects, and whether all are ambient
		bool writeParticles(Buffer& buf, const GameData& gameData) const;
		bool allAmbient() const;
		bool visualsDirty = true; // LivingEntity.effectsDirty

		void save(Buffer& buf) const;
		void load(Buffer& buf);
		// Vanilla's "active_effects" list (player data)
		void loadNbt(const nbt::TagList& list);
		void saveNbt(nbt::TagList& list) const;

	  private:
		std::vector<Instance> _effects;

		void onAdded(Host& host, const Instance& instance);
		void onUpdated(Host& host, const Instance& instance, bool reapply);
		void onRemoved(Host& host, const Instance& instance);
		void applyModifiers(Host& host, const Instance& instance);
		void removeModifiers(Host& host, Effect effect);
		// MobEffect.onEffectStarted, onEffectAdded
		void onStarted(Host& host, const Instance& instance);
		// MobEffect.shouldApplyEffectTickThisTick / applyEffectTick: false removes the effect
		bool applyTick(Host& host, const Instance& instance);
	};

	// Potions: the effects of a potion (minecraft:potion registry name), none for water, awkward...
	std::vector<Instance> potionEffects(const std::string& potion);
	// PotionContents.applyToLivingEntity: instant effects at once (source: who threw or drank it), the others added.
	// durationScale: potion_duration_scale
	void applyPotion(Host& host, Active& effects, const std::string& potion, float durationScale, Actor* source);

	// MobEffect.applyInstantenousEffect: instant health / damage from a potion (effectiveness: the splash distance
	// factor), others as one tick
	void applyInstantaneous(Host& host, const Instance& instance, double effectiveness, Actor* source = nullptr, Actor* direct = nullptr);
	bool shouldApplyEffectTickThisTick(Effect effect, int tick, int amplifier);
} // namespace MobEffects

#endif
