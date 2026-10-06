#include "player.hpp"
#include "world/inventory/Menu.hpp"

#include "lib/UUID.hpp"
#include "network/server.hpp"
#include "PacketIds.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "world/ChunkStreamer.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/Survival.hpp"

#include <chrono>
#include <string>

namespace {
	// RandomSource.create(): a seed of its own for each entity
	int64_t newSeed(int id) { return std::chrono::steady_clock::now().time_since_epoch().count() ^ (static_cast<int64_t>(id) * 0x9E3779B97F4A7C15LL); }
} // namespace

Player::Player(Server& server)
	: _name("Player_entity"), _state(PlayerState::None), _socketFd(-1), _disconnected(false), x(0), y(0), z(0), health(0), _uuid(),
	  _playerId(server.getIdManager().allocate()), _server(server), _config(new PlayerConfig()) {
	_random.setSeed(newSeed(_playerId));
}

Player::Player(const std::string& name, const PlayerState state, const int socket, Server& server)
	: _state(state), _socketFd(socket), _disconnected(false), x(0), y(0), z(0), health(20), _uuid(), _playerId(server.getIdManager().allocate()), _server(server),
	  _config(new PlayerConfig()) {
	_random.setSeed(newSeed(_playerId));
	if (name.length() > 32)
		_name = name.substr(0, 31);
	else
		_name = name;
}

Player& Player::operator=(const Player& src) {
	if (this != &src) {
		this->_name		= src._name;
		this->_socketFd = src._socketFd;
		this->health	= src.health;
		this->x			= src.x;
		this->y			= src.y;
		this->z			= src.z;
	}
	return (*this);
}

Player::~Player() {
	_chunkStreamer.reset(); // Releases its chunks while the player is still whole
	_server.getIdManager().release(_playerId);
	delete _config;
}

std::string Player::getPlayerName(void) {
	std::lock_guard<std::mutex> lock(_nameMutex);
	return _name;
}

void Player::setPlayerName(const std::string& name) {
	std::lock_guard<std::mutex> lock(_nameMutex);
	_name = name;
}
PlayerState Player::getPlayerState() { return (this->_state); }
void		Player::setPlayerState(PlayerState state) { this->_state = state; }
void		Player::setSocketFd(int socket) { this->_socketFd = socket; }
int			Player::getSocketFd() const { return (this->_socketFd); }

void Player::setUUID(UUID uuid) { _uuid = uuid; }

void Player::createChunkStreamer() { _chunkStreamer = std::make_unique<ChunkStreamer>(_server, *this); }

int Player::getPlayerID() const { return (_playerId); }

// PlayerConfig implementation
PlayerConfig::PlayerConfig()
	: _chatMode(0), _mainHand(1), _locale("en_US"), _viewDistance(10), _displayedSkinParts(0), _chatColors(true), _enableTextFiltering(false),
	  _allowServerListings(true) {}

PlayerConfig::~PlayerConfig() {}

// ----- Actor -----

int Player::typeId() const {
	static int playerType = -2;
	if (playerType == -2 && _level) playerType = _level->gameData().getStaticId("minecraft:entity_type", "minecraft:player");
	return playerType < 0 ? -1 : playerType;
}

AABB Player::boundingBox() const {
	// Avatar.POSES: the size of each pose
	double width = BB_WIDTH, height = BB_HEIGHT;
	switch (_survival.pose) {
	case Pose::Crouching: height = 1.5F; break;
	case Pose::Swimming:
	case Pose::FallFlying:
	case Pose::SpinAttack: height = 0.6F; break;
	case Pose::Sleeping:
	case Pose::Dying: width = height = 0.2F; break;
	default: break;
	}
	double half = width / 2.0;
	return {_pos.x - half, _pos.y, _pos.z - half, _pos.x + half, _pos.y + height, _pos.z + half};
}

double Player::eyeY() const { return _pos.y + Survival::eyeHeight(*this); }

bool Player::hurtServer(const Combat::DamageSource& source, float amount) { return Combat::damage(_server, *this, amount, source); }

void Player::igniteForTicks(int ticks) { Survival::igniteForTicks(*this, ticks); }

void Player::pushMotion(const Vec3& impulse) {
	if (isDisconnected()) return;
	Buffer motion;
	motion.writeVarInt(_playerId);
	motion.writeLpVec3(impulse.x, impulse.y, impulse.z);
	_server.getPlayerTracker().broadcast(this, PacketId::Play::Clientbound::SET_ENTITY_MOTION, motion, true);
}

// ----- Attributes and effects -----

AttributeMap& Player::attributes() {
	if (!_attributes) {
		static const GameData::EntityTypeInfo none; // No game data: no attributes
		const GameData&						  data = effectGameData();
		const GameData::EntityTypeInfo*		  type = data.getEntityType(data.getStaticId("minecraft:entity_type", "minecraft:player"));
		_attributes = std::make_unique<AttributeMap>(data, type ? *type : none);
	}
	return *_attributes;
}

double Player::getAttributeValue(int attribute) {
	if (attribute < 0 || static_cast<size_t>(attribute) >= effectGameData().getAttributeCount()) return 0.0;
	return attributes().getValue(attribute);
}

float Player::maxHealth() {
	int id = effectGameData().getStaticId("minecraft:attribute", "minecraft:max_health");
	return id < 0 ? 20.0F : static_cast<float>(getAttributeValue(id));
}

double Player::attributeValueWithBase(int attribute, double base) {
	// AttributeInstance.calculateValue with that base
	const GameData& data = effectGameData();
	if (attribute < 0 || static_cast<size_t>(attribute) >= data.getAttributeCount()) return base;
	AttributeInstance* instance = attributes().getInstance(attribute);
	double			   value	= base;
	if (instance) {
		for (const AttributeModifier& m : instance->modifiers()) {
			if (m.operation == AttributeModifier::Operation::AddValue) value += m.amount;
		}
		double total = value;
		for (const AttributeModifier& m : instance->modifiers()) {
			if (m.operation == AttributeModifier::Operation::AddMultipliedBase) total += value * m.amount;
		}
		for (const AttributeModifier& m : instance->modifiers()) {
			if (m.operation == AttributeModifier::Operation::AddMultipliedTotal) total *= 1.0 + m.amount;
		}
		value = total;
	}
	const GameData::AttributeInfo& info = data.getAttribute(attribute);
	return std::clamp(value, info.minValue, info.maxValue);
}

void Player::setAbsorptionAmount(float amount) {
	int	  maxAbsorption = effectGameData().getStaticId("minecraft:attribute", "minecraft:max_absorption");
	float max			= maxAbsorption < 0 ? amount : static_cast<float>(getAttributeValue(maxAbsorption));
	float clamped		= std::clamp(amount, 0.0F, std::max(max, 0.0F));
	if (clamped == _absorption) return;
	_absorption = clamped;
	_survival.dirtyData |= Survival::DATA_ABSORPTION;
}

void Player::sendActiveEffects() {
	for (const MobEffects::Instance& instance : _effects.all()) sendEffectUpdate(instance, false);
}

Level& Player::effectLevel() { return _level ? *_level : _server.getLevel(); }

// The level's (the server's before the player is in one)
const GameData& Player::effectGameData() const { return _level ? _level->gameData() : _server.getGameData(); }

int Player::effectTypeId() const { return effectGameData().getStaticId("minecraft:entity_type", "minecraft:player"); }

void Player::effectHeal(float amount) { Survival::heal(*this, amount); }

bool Player::effectHurt(const Combat::DamageSource& source, float amount) { return Combat::damage(_server, *this, amount, source); }

bool Player::effectCanBeAffected(MobEffects::Effect effect) const { return MobEffects::Active::canBeAffected(effectGameData(), effectTypeId(), effect); }

void Player::onEffectAttributesChanged() {
	if (_combat.health > maxHealth()) _combat.health = maxHealth();
	setAbsorptionAmount(_absorption);
}

void Player::effectFoodExhaustion(float exhaustion) { Survival::causeFoodExhaustion(*this, exhaustion); }

// FoodData.eat(int, float): the saturation from its modifier (FoodConstants.saturationByModifier)
void Player::effectEat(int food, float saturation) { _foodData.eat(food, static_cast<float>(food) * saturation * 2.0F); }

void Player::sendEffectUpdate(const MobEffects::Instance& instance, bool blend) {
	// ClientboundUpdateMobEffectPacket, to its own client
	Buffer buf;
	buf.writeVarInt(_playerId);
	buf.writeVarInt(instance.effect);
	buf.writeVarInt(instance.amplifier);
	buf.writeVarInt(instance.duration);
	buf.writeUByte(instance.packetFlags(blend));
	Packet::send(shared_from_this(), PacketId::Play::Clientbound::UPDATE_MOB_EFFECT, buf, _server);
}

void Player::sendEffectRemoved(MobEffects::Effect effect) {
	Buffer buf;
	buf.writeVarInt(_playerId);
	buf.writeVarInt(effect);
	Packet::send(shared_from_this(), PacketId::Play::Clientbound::REMOVE_MOB_EFFECT, buf, _server);
}

void Player::updateEffectVisuals() {
	// updateInvisibilityStatus: the shared flags follow at the next tick (computeSharedFlags), the particles now
	_survival.dirtyData |= Survival::DATA_EFFECTS;
	if (!_effects.empty()) _survival.effectAmbience = _effects.allAmbient();
}

void Player::playEffectSound(const std::string& sound) {
	effectLevel().playSoundAt(nullptr, _pos.x, _pos.y, _pos.z, sound, Level::SoundSource::Players, 1.0F, 1.0F);
}
