#include "world/entity/MobRegistry.hpp"

#include "data/GameData.hpp"
#include "world/Level.hpp"
#include "world/entity/Mob.hpp"
#include "world/entity/mobs/Animal.hpp"
#include "world/entity/mobs/Monster.hpp"

#include <stdexcept>

int MobRegistry::typeId(const std::string& type) const {
	int id = _gameData.getStaticId("minecraft:entity_type", type);
	if (id < 0) throw std::runtime_error("unknown entity type " + type);
	return id;
}

void MobRegistry::setFactory(const std::string& type, Factory factory) { _factories[typeId(type)] = std::move(factory); }

void MobRegistry::addGoals(const std::string& type, GoalRegistrar registrar) { _goals[typeId(type)].push_back(std::move(registrar)); }

std::vector<int> MobRegistry::typesOf(const std::string& javaClass) const {
	std::vector<int> types;
	for (int id = 0;; id++) {
		const GameData::EntityTypeInfo* type = _gameData.getEntityType(id);
		if (!type) break;
		if (type->is(javaClass)) types.push_back(id);
	}
	return types;
}

std::unique_ptr<Mob> MobRegistry::create(Level& level, int typeId) const {
	const GameData::EntityTypeInfo* type = _gameData.getEntityType(typeId);
	if (!type || !type->mob || type->attributes.empty()) return nullptr;
	auto				 factory = _factories.find(typeId);
	std::unique_ptr<Mob> mob;
	if (factory != _factories.end()) {
		mob = factory->second(level, typeId);
	} else if (type->is("Animal")) {
		mob = std::make_unique<Animal>(level, typeId); // Ages, falls in love and breeds, without its goals yet
	} else if (type->is("Monster")) {
		mob = std::make_unique<Monster>(level, typeId);
	} else {
		mob = std::make_unique<Mob>(level, typeId);
	}
	if (!mob) return nullptr;
	// Mob.registerGoals, on the server only: the class's own, then the ones registered for the type
	mob->registerGoals();
	auto goals = _goals.find(typeId);
	if (goals != _goals.end()) {
		for (const GoalRegistrar& registrar : goals->second) registrar(*mob);
	}
	return mob;
}

Mob* MobRegistry::spawn(Level& level, int typeId, const BlockPos& pos, SpawnReason reason, bool shouldOffsetY, bool shouldOffsetYMore) const {
	std::unique_ptr<Mob> mob = create(level, typeId);
	if (!mob) return nullptr;
	double yOffset = 0.0;
	if (shouldOffsetY) {
		// EntityType.getYOffset: lowered onto the blocks of pos (and below, if asked) from one block up
		Vec3 above{pos.x + 0.5, pos.y + 1.0, pos.z + 0.5};
		mob->setPos(above);
		AABB area{static_cast<double>(pos.x), static_cast<double>(pos.y), static_cast<double>(pos.z), pos.x + 1.0, pos.y + 1.0, pos.z + 1.0};
		if (shouldOffsetYMore) area.minY -= 1.0;
		yOffset = 1.0 + Entity::collideDown(level, mob->boundingBox(), area, shouldOffsetYMore ? -2.0 : -1.0);
	}
	float yRot = Mth::wrapDegrees(level.random().nextFloat() * 360.0F);
	mob->snapTo({pos.x + 0.5, pos.y + yOffset, pos.z + 0.5}, yRot, 0.0F);
	mob->setYHeadRot(yRot);
	mob->setYBodyRot(yRot);
	DifficultyInstance difficulty = level.getCurrentDifficultyAt(pos);
	mob->finalizeSpawn(difficulty, static_cast<int>(reason));
	Mob* added = static_cast<Mob*>(level.addFreshEntity(std::move(mob)));
	added->playAmbientSound();
	return added;
}
