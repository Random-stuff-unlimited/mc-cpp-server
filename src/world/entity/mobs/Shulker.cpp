#include "world/entity/mobs/Shulker.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Clip.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/ai/Goals.hpp"

#include <cmath>
#include <stdexcept>

namespace {
	constexpr int		  SAVE_VERSION		   = 1;
	constexpr int		  SERIALIZER_DIRECTION = 12;
	constexpr const char* COVERED_ARMOR		   = "minecraft:covered";

	// Shulker.ShulkerAttackGoal: wide open, a bullet every 1 to 5.5 seconds at its target within 20 blocks
	class ShulkerAttackGoal : public Goal {
	  public:
		explicit ShulkerAttackGoal(Shulker& shulker) : _shulker(shulker) { setFlags({Flag::Move, Flag::Look}); }
		bool canUse() override {
			Actor* target = _shulker.getTarget();
			return target && target->isAlive() && _shulker.level().difficulty() != 0;
		}
		void start() override {
			_attackTime = 20;
			_shulker.setRawPeekAmount(100);
		}
		void stop() override { _shulker.setRawPeekAmount(0); }
		bool requiresUpdateEveryTick() override { return true; }
		void tick() override {
			if (_shulker.level().difficulty() == 0) return;
			_attackTime--;
			Actor* target = _shulker.getTarget();
			if (!target) return;
			_shulker.lookControl().setLookAt(*target, 180.0F, 180.0F);
			if (_shulker.distanceToSqr(*target) < 400.0) {
				if (_attackTime <= 0) {
					_attackTime = 20 + _shulker.random().nextInt(10) * 20 / 2;
					_shulker.level().entities().add(std::make_unique<ShulkerBullet>(_shulker.level(), &_shulker, target));
					_shulker.playSound("minecraft:entity.shulker.shoot", 2.0F, (_shulker.random().nextFloat() - _shulker.random().nextFloat()) * 0.2F + 1.0F);
				}
			} else {
				_shulker.setTarget(nullptr);
			}
		}

	  private:
		Shulker& _shulker;
		int		 _attackTime = 0;
	};

	// Shulker.ShulkerPeekGoal: with nothing to do, it peeks out for 1 to 3 seconds
	class ShulkerPeekGoal : public Goal {
	  public:
		explicit ShulkerPeekGoal(Shulker& shulker) : _shulker(shulker) {}
		bool canUse() override { return !_shulker.getTarget() && _shulker.random().nextInt(reducedTickDelay(40)) == 0; }
		bool canContinueToUse() override { return !_shulker.getTarget() && _peekTime > 0; }
		void start() override {
			_peekTime = adjustedTickDelay(20 * (1 + _shulker.random().nextInt(3)));
			_shulker.setRawPeekAmount(30);
		}
		void stop() override {
			if (!_shulker.getTarget()) _shulker.setRawPeekAmount(0);
		}
		void tick() override { _peekTime--; }

	  private:
		Shulker& _shulker;
		int		 _peekTime = 0;
	};
} // namespace

// ===================== Shulker =====================

Shulker::Shulker(Level& level, int typeId) : Monster(level, typeId) {
	_xpReward = 5;
	setNoGravity(true);
}

void Shulker::registerGoals() {
	_goalSelector.addGoal(1, std::make_unique<LookAtPlayerGoal>(*this, "Player", 8.0F, 0.02F, true));
	_goalSelector.addGoal(4, std::make_unique<ShulkerAttackGoal>(*this));
	_goalSelector.addGoal(7, std::make_unique<ShulkerPeekGoal>(*this));
	_goalSelector.addGoal(8, std::make_unique<RandomLookAroundGoal>(*this));
	auto hurtBy = std::make_unique<HurtByTargetGoal>(*this, std::vector<std::string>{"Shulker"});
	hurtBy->setAlertOthers();
	_targetSelector.addGoal(1, std::move(hurtBy));
	_targetSelector.addGoal(2, std::make_unique<NearestAttackableTargetGoal>(*this, "Player", true));
}

void Shulker::setAttachFace(Direction face) {
	if (face == _attachFace) return;
	_attachFace = face;
	markData(DATA_ATTACH_FACE);
}

void Shulker::setRawPeekAmount(int peek) {
	peek = std::max(0, std::min(100, peek));
	if (peek == _peek) return;
	// Closed, its shell gives 20 armor more
	if (AttributeInstance* armor = _attributes.getInstance(_ids.armor)) {
		armor->removeModifier(COVERED_ARMOR);
		if (peek == 0) armor->addModifier({COVERED_ARMOR, 20.0, AttributeModifier::Operation::AddValue}, false);
	}
	playSound(peek == 0 ? "minecraft:entity.shulker.close" : (_peek == 0 ? "minecraft:entity.shulker.open" : ""), 1.0F, 1.0F);
	_peek = peek;
	markData(DATA_PEEK);
}

void Shulker::setColor(int color) {
	if (color == _color) return;
	_color = color;
	markData(DATA_COLOR);
}

bool Shulker::canStayAt(const BlockPos& pos, Direction face) {
	// Shulker.canStayAt: an empty spot, the neighbor on that side sturdy toward it
	if (!_level.blocks().isAir(_level.getBlockState(pos))) return false;
	BlockPos neighbor = pos.relative(face);
	return _level.gameData().isFaceSturdy(_level.getBlockState(neighbor), static_cast<int>(Directions::opposite(face)), GameData::SupportType::Full);
}

std::optional<Direction> Shulker::findAttachableSurface(const BlockPos& pos) {
	if (canStayAt(pos, _attachFace)) return _attachFace;
	for (Direction face : Directions::ALL) {
		if (canStayAt(pos, face)) return face;
	}
	return std::nullopt;
}

bool Shulker::teleportSomewhere() {
	if (isNoAi() || !isAlive()) return false;
	BlockPos here = blockPosition();
	for (int i = 0; i < 5; i++) {
		BlockPos pos = here.offset(Mth::nextInt(_random, -8, 8), Mth::nextInt(_random, -8, 8), Mth::nextInt(_random, -8, 8));
		if (pos.y <= _level.minY()) continue;
		std::optional<Direction> face = findAttachableSurface(pos);
		if (!face) continue;
		AABB box = boundingBox().move(pos.x + 0.5 - _position.x, pos.y - _position.y, pos.z + 0.5 - _position.z);
		if (_level.hasBlockCollision(box.deflate(1.0E-6)) || !_level.actorsIn(box, this).empty()) continue;
		setPos({pos.x + 0.5, static_cast<double>(pos.y), pos.z + 0.5});
		_oldPosition = _position;
		setAttachFace(*face);
		setRawPeekAmount(0);
		setTarget(nullptr);
		playSound("minecraft:entity.shulker.teleport", 1.0F, 1.0F);
		return true;
	}
	return false;
}

void Shulker::tick() {
	Monster::tick();
	if (isRemoved()) return;
	_delta = {};
	// Its block gone: another face here, else somewhere else
	if (!isNoAi() && !canStayAt(blockPosition(), _attachFace)) {
		if (std::optional<Direction> face = findAttachableSurface(blockPosition())) {
			setAttachFace(*face);
		} else {
			teleportSomewhere();
		}
	}
}

bool Shulker::hurtServer(const Combat::DamageSource& source, float amount) {
	if (isClosed()) {
		// Arrows bounce off its shell
		Actor* direct = source.directEntity();
		if (direct && direct != source.causing && _level.gameData().getEntityType(direct->typeId())->is("AbstractArrow")) return false;
	}
	bool hurt = Monster::hurtServer(source, amount);
	if (hurt && isAlive() && _health < maxHealth() * 0.5F && _random.nextInt(4) == 0) teleportSomewhere();
	return hurt;
}

std::shared_ptr<SpawnGroupData> Shulker::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	// Snapped to its block, stuck to whatever is next to it (the floor first)
	BlockPos pos = blockPosition();
	setPos({pos.x + 0.5, static_cast<double>(pos.y), pos.z + 0.5});
	_oldPosition = _position;
	if (std::optional<Direction> face = findAttachableSurface(pos)) setAttachFace(*face);
	setYRotPublic(0.0F);
	setYBodyRot(0.0F);
	return Monster::finalizeSpawn(difficulty, reason, group);
}

void Shulker::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Monster::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_ATTACH_FACE, onlyNonDefault, _attachFace == Direction::Down)) {
		buf.writeUByte(static_cast<uint8_t>(DATA_ATTACH_FACE));
		buf.writeVarInt(SERIALIZER_DIRECTION);
		buf.writeVarInt(static_cast<int>(_attachFace));
	}
	if (wantsData(mask, DATA_PEEK, onlyNonDefault, _peek == 0)) writeByteData(buf, DATA_PEEK, static_cast<uint8_t>(_peek));
	if (wantsData(mask, DATA_COLOR, onlyNonDefault, _color == NO_COLOR)) writeByteData(buf, DATA_COLOR, static_cast<uint8_t>(_color));
}

void Shulker::save(Buffer& buf) const {
	Monster::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeUByte(static_cast<uint8_t>(_attachFace));
	buf.writeUByte(static_cast<uint8_t>(_peek));
	buf.writeUByte(static_cast<uint8_t>(_color));
}

void Shulker::load(Buffer& buf) {
	Monster::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown shulker data version");
	setAttachFace(static_cast<Direction>(buf.readUByte() % 6));
	setRawPeekAmount(buf.readUByte());
	setColor(buf.readUByte());
}

// ===================== ShulkerBullet =====================

ShulkerBullet::ShulkerBullet(Level& level, Shulker* shulker, Actor* target)
	: Projectile(level, level.gameData().getStaticId("minecraft:entity_type", "minecraft:shulker_bullet"), 0.3125F, 0.3125F) {
	if (shulker) {
		setOwner(shulker);
		// From the middle of the shulker's face, toward its target
		_position	 = {shulker->position().x, shulker->position().y + 0.5, shulker->position().z};
		_oldPosition = _position;
	}
	if (target) _target = EntityRef(*target);
	_hasBeenShot = true;
}

void ShulkerBullet::tick() {
	Actor* target = _target.isSet() ? _level.actorByRef(_target) : nullptr;
	if (target && !target->isAlive()) target = nullptr;
	if (!target) {
		// Its target gone, it falls
		_delta.y -= 0.04;
	} else {
		// It steers toward the middle of its target, at 0.15 blocks a tick (vanilla turns along the axes)
		AABB box	 = target->boundingBox();
		Vec3 aim	 = Vec3{target->position().x, target->position().y + (box.maxY - box.minY) * 0.5, target->position().z} - _position;
		Vec3 wanted	 = aim.normalize().scale(0.15);
		_delta		 = _delta + (wanted - _delta).scale(0.2);
	}
	Vec3			to	  = _position + _delta;
	Clip::HitResult block = Clip::clip(_level, _position, to, Clip::BlockMode::Collider, Clip::FluidMode::None);
	if (block.hit) to = block.location;
	std::optional<EntityHit> entity = findHitEntity(_position, to, boundingBox().expandTowards(_delta).inflate(1.0), computeMargin());
	_position						= entity ? entity->location : to;
	updateRotation();
	Projectile::tick();
	if (isRemoved()) return;
	if (entity) {
		onHit(entity->actor);
	} else if (block.hit || ++_life > 400) {
		onHit(nullptr);
	}
}

void ShulkerBullet::onHit(Actor* actor) {
	if (actor) {
		Actor*				 by = owner();
		Combat::DamageSource source{"minecraft:mob_projectile", by, this, std::nullopt};
		bool				 hurt = false;
		if (Player* player = actor->asPlayer()) {
			hurt = Combat::damage(_level.server(), *player, 4.0F, source);
			if (hurt) player->addEffect({MobEffects::Levitation, 200, 0});
		} else {
			hurt = actor->hurtServer(source, 4.0F);
			if (hurt) {
				if (LivingEntity* living = actor->asLiving()) living->addEffect({MobEffects::Levitation, 200, 0});
			}
		}
	}
	_level.playSoundAt(nullptr, _position.x, _position.y, _position.z, "minecraft:entity.shulker_bullet.hit", Level::SoundSource::Hostile, 1.0F, 1.0F);
	discard();
}

bool ShulkerBullet::hurtServer(const Combat::DamageSource&, float) {
	// One hit and it bursts
	_level.playSoundAt(nullptr, _position.x, _position.y, _position.z, "minecraft:entity.shulker_bullet.hurt", Level::SoundSource::Hostile, 1.0F, 1.0F);
	discard();
	return true;
}
