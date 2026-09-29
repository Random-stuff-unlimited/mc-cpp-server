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
