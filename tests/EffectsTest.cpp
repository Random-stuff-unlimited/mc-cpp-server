#include "LevelFixture.hpp"
#include "Test.hpp"
#include "network/buffer.hpp"
#include "world/Combat.hpp"
#include "world/entity/Mob.hpp"
#include "world/entity/MobEffects.hpp"

#include <cmath>

namespace {
	constexpr int Y = LevelFixture::SURFACE + 1;

	// Without its goals, at night, standing still
	Mob* spawn(LevelFixture& f, const char* name) {
		f.world->setDayTime(18000);
		Mob* mob = f.level->mobs().spawn(*f.level, f.data.getStaticId("minecraft:entity_type", name), {0, Y, 0}, MobRegistry::SpawnReason::Command);
		if (mob) {
			mob->setBaby(false);
			mob->goalSelector().removeAllGoals();
			mob->targetSelector().removeAllGoals();
			mob->removeAllEffects(); // Spiders may spawn with one
		}
		return mob;
	}

	using MobEffects::Instance;
} // namespace

// The effects' ids follow the minecraft:mob_effect registry
TEST(effects_registry_order) {
	LevelFixture f(1);
	MobEffects::verify(f.data);
	CHECK(MobEffects::byName("poison") == MobEffects::Poison);
	CHECK(MobEffects::byName("minecraft:infested") == MobEffects::Infested);
	CHECK(MobEffects::byName("nope") == MobEffects::Count);
}

// MobEffectInstance.update: a stronger shorter effect hides the longer weaker one, which comes back after it
TEST(effects_stronger_hides_weaker) {
	LevelFixture f(1);
	Mob* cow = spawn(f, "minecraft:cow");
	CHECK(cow);
	if (!cow) return;
	CHECK(cow->addEffect(Instance(MobEffects::Speed, 1000, 0)));
	CHECK(cow->addEffect(Instance(MobEffects::Speed, 100, 1)));
	const Instance* speed = cow->effects().get(MobEffects::Speed);
	CHECK(speed && speed->amplifier == 1 && speed->duration == 100 && speed->hidden && speed->hidden->duration == 1000);
	// A weaker one that doesn't last longer changes nothing
	CHECK(!cow->addEffect(Instance(MobEffects::Speed, 50, 0)));
	f.tick(100);
	speed = cow->effects().get(MobEffects::Speed);
	CHECK(speed && speed->amplifier == 0 && speed->duration == 900 && !speed->hidden);
}

// Speed and slowness change the movement speed attribute while they last
TEST(effects_attribute_modifiers) {
	LevelFixture f(1);
	Mob* cow = spawn(f, "minecraft:cow");
	CHECK(cow);
	if (!cow) return;
	int	   speed = cow->attributeIds().movementSpeed;
	double base	 = cow->getAttributeValue(speed);
	cow->addEffect(Instance(MobEffects::Speed, 10, 1)); // +40 % of the total
	CHECK(std::abs(cow->getAttributeValue(speed) - base * (1.0 + 0.2F * 2)) < 1e-12); // 0.2F, a float like vanilla
	f.tick(11);
	CHECK(!cow->hasEffect(MobEffects::Speed));
	CHECK_EQ(cow->getAttributeValue(speed), base);
}

// Poison: 1 damage every 25 ticks (level I), never below 1 health
TEST(effects_poison_damage) {
	LevelFixture f(1);
	Mob* cow = spawn(f, "minecraft:cow");
	CHECK(cow);
	if (!cow) return;
	f.tick(1);
	cow->addEffect(Instance(MobEffects::Poison, 100, 0));
	f.tick(100);
	CHECK(!cow->hasEffect(MobEffects::Poison));
	CHECK_EQ(cow->health(), 6.0F); // Hurt at 100, 75, 50 and 25 ticks left
	cow->setHealth(1.0F);
	cow->addEffect(Instance(MobEffects::Poison, 100, 3));
	f.tick(100);
	CHECK_EQ(cow->health(), 1.0F);
}

// Undead ignore poison and regeneration, and harming heals them; spiders ignore poison
TEST(effects_immunities) {
	LevelFixture f(1);
	Mob* zombie = spawn(f, "minecraft:zombie");
	Mob* spider = spawn(f, "minecraft:spider");
	CHECK(zombie && spider);
	if (!zombie || !spider) return;
	CHECK(!zombie->addEffect(Instance(MobEffects::Poison, 100)));
	CHECK(!zombie->addEffect(Instance(MobEffects::Regeneration, 100)));
	CHECK(zombie->addEffect(Instance(MobEffects::Speed, 100)));
	CHECK(!spider->addEffect(Instance(MobEffects::Poison, 100)));
	zombie->setHealth(10.0F);
	MobEffects::applyInstantaneous(*zombie, Instance(MobEffects::InstantDamage, 1, 0), 1.0);
	CHECK_EQ(zombie->health(), 14.0F);
}

// Resistance takes 20 % off per level: immune at level V
TEST(effects_resistance) {
	LevelFixture f(1);
	Mob* cow = spawn(f, "minecraft:cow");
	CHECK(cow);
	if (!cow) return;
	cow->addEffect(Instance(MobEffects::Resistance, 100, 4));
	cow->hurtServer({"minecraft:generic", nullptr}, 5.0F);
	CHECK_EQ(cow->health(), 10.0F);
	cow->removeEffect(MobEffects::Resistance);
	cow->addEffect(Instance(MobEffects::Resistance, 100, 1)); // 40 %
	f.tick(25);
	cow->hurtServer({"minecraft:generic", nullptr}, 5.0F);
	CHECK_EQ(cow->health(), 7.0F);
	// Fire resistance: no fire damage at all
	cow->addEffect(Instance(MobEffects::FireResistance, 100));
	f.tick(25);
	CHECK(!cow->hurtServer({"minecraft:in_fire", nullptr}, 5.0F));
}

// The effects are saved with the mob
TEST(effects_saved) {
	LevelFixture f(1);
	Mob* cow = spawn(f, "minecraft:cow");
	CHECK(cow);
	if (!cow) return;
	cow->addEffect(Instance(MobEffects::Slowness, 400, 2, true, false));
	cow->addEffect(Instance(MobEffects::Slowness, 50, 3));
	Buffer buf;
	cow->save(buf);
	Mob* other = spawn(f, "minecraft:cow");
	CHECK(other);
	if (!other) return;
	other->load(buf);
	const Instance* slowness = other->effects().get(MobEffects::Slowness);
	CHECK(slowness && slowness->amplifier == 3 && slowness->duration == 50 && slowness->hidden);
	CHECK(slowness && slowness->hidden && slowness->hidden->amplifier == 2 && slowness->hidden->duration == 400 && !slowness->hidden->visible);
	// The modifier came with the attributes
	CHECK(other->getAttributeValue(other->attributeIds().movementSpeed) < cow->getAttributeValue(cow->attributeIds().movementSpeed) + 1e-9);
}
