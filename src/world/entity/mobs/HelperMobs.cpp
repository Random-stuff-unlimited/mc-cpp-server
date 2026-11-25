#include "world/entity/mobs/HelperMobs.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/blockentity/BlockEntity.hpp"
#include "world/entity/ItemEntity.hpp"
#include "world/entity/ai/Controls.hpp"
#include "world/entity/ai/Goals.hpp"
#include "world/entity/ai/RandomPos.hpp"
#include "world/entity/mobs/FlyingAnimals.hpp"

#include <cmath>
#include <stdexcept>

namespace {
	constexpr int SAVE_VERSION						= 1;
	constexpr int SERIALIZER_COPPER_GOLEM_STATE	= 32, SERIALIZER_WEATHERING_COPPER_STATE = 33;

	void writeStack(Buffer& buf, const ItemStack& stack) {
		buf.writeVarInt(stack.isEmpty() ? 0 : stack.item);
		buf.writeVarInt(stack.isEmpty() ? 0 : stack.count);
		buf.writeVarInt(static_cast<int32_t>(stack.components.size()));
		buf.writeBytes(stack.components);
	}
	ItemStack readStack(Buffer& buf) {
		ItemStack stack;
		stack.item		 = buf.readVarInt();
		stack.count		 = buf.readVarInt();
		stack.components = buf.readBytes(static_cast<size_t>(buf.readVarInt()));
		return stack.isEmpty() ? ItemStack{} : stack;
	}
	int maxStack(Level& level, const ItemStack& stack) {
		const GameData::ItemProperties* props = level.gameData().getItemProperties(stack.item);
		return props ? props->maxStackSize : 64;
	}
	bool isItemIn(Level& level, const ItemStack& stack, const char* tag) {
		return !stack.isEmpty() && level.gameData().isInTag("minecraft:item", tag, stack.item);
	}
	bool isItem(Level& level, const ItemStack& stack, const char* name) {
		return !stack.isEmpty() && stack.item == level.gameData().getStaticId("minecraft:item", name);
	}
	void giveToPlayer(Level& level, Player& player, ItemStack stack) {
		if (stack.isEmpty()) return;
		if (!player.inventory().add(stack, player.getSelectedSlot(), player.isCreative(), level.gameData()) && !stack.isEmpty()) {
			level.entities().add(std::make_unique<ItemEntity>(level, player.position(), std::move(stack), Vec3{}));
		}
	}

	// ----- Allay -----

	// AllayAi's GoAndGiveItemsToTarget: with items, to its player; close enough, it throws them at its feet
	class AllayGiveGoal : public Goal {
	  public:
		explicit AllayGiveGoal(Allay& allay) : _allay(allay) { setFlags({Flag::Move, Flag::Look}); }
		bool canUse() override { return !_allay.inventory().isEmpty() && _allay.likedPlayer(); }
		void tick() override {
			Player* player = _allay.likedPlayer();
			if (!player) return;
			_allay.lookControl().setLookAt(*player, 30.0F, 30.0F);
			if (_allay.distanceToSqr(*player) > 3.0 * 3.0) {
				if (_allay.navigation().isDone() || _allay.tickCount() % 20 == 0) _allay.navigation().moveTo(*player, 2.25);
				return;
			}
			// BehaviorUtils.throwItem: toward the player, a little up
			_allay.navigation().stop();
			Vec3 from{_allay.position().x, _allay.eyeY() - 0.3, _allay.position().z};
			Vec3 to	 = (player->position() + Vec3{0.0, 1.0, 0.0} - from).normalize().scale(0.3);
			auto item = std::make_unique<ItemEntity>(_allay.level(), from, _allay.inventory(), to);
			item->setPickupDelay(40);
			_allay.level().entities().add(std::move(item));
			_allay.inventoryMutable() = ItemStack{};
			_allay.throwCooldown	   = Allay::THROW_COOLDOWN;
			_allay.playSound("minecraft:entity.allay.item_thrown", 1.0F, 1.0F);
		}

	  private:
		Allay& _allay;
	};

	// AllayAi's GoToWantedItem: the nearest item like the one it holds, within 32 blocks
	class AllayCollectGoal : public Goal {
	  public:
		explicit AllayCollectGoal(Allay& allay) : _allay(allay) { setFlags({Flag::Move}); }
		bool canUse() override {
			if (!_allay.hasItemInHand() || _allay.throwCooldown > 0) return false;
			_item = nullptr;
			double best = 0.0;
			for (Actor* actor : _allay.level().actorsIn(_allay.boundingBox().inflate(32.0, 16.0, 32.0), &_allay)) {
				auto* item = dynamic_cast<ItemEntity*>(actor);
				if (!item || item->isRemoved() || item->pickupDelay() > 0 || !_allay.wantsItem(item->item())) continue;
				double d = item->distanceToSqr(_allay);
				if (!_item || d < best) _item = item, best = d;
			}
			if (_item) _itemId = _item->id();
			return _item != nullptr;
		}
		bool canContinueToUse() override {
			Actor* actor = _allay.level().actorById(_itemId);
			return actor && !static_cast<Entity*>(actor->asEntity())->isRemoved() && _allay.wantsItem(static_cast<ItemEntity*>(actor->asEntity())->item());
		}
		void tick() override {
			auto* item = dynamic_cast<ItemEntity*>(_allay.level().actorById(_itemId));
			if (!item) return;
			if (_allay.boundingBox().inflate(1.0, 0.5, 1.0).intersects(item->boundingBox())) {
				int taken = _allay.addToInventory(item->item());
				if (taken > 0) {
					ItemStack left = item->item();
					left.shrink(taken);
					if (left.isEmpty()) {
						item->discard();
					} else {
						item->setItem(left);
					}
					_allay.playSound("minecraft:entity.item.pickup", 0.2F, 1.0F);
				}
				return;
			}
			if (_allay.navigation().isDone() || _allay.tickCount() % 10 == 0) _allay.navigation().moveTo(item->position().x, item->position().y + 0.5, item->position().z, 1.75);
		}

	  private:
		Allay&		_allay;
		ItemEntity* _item	= nullptr;
		int			_itemId = -1;
	};

	// AllayAi's StayCloseToTarget: within 16 blocks of its player (it comes back to 4)
	class AllayStayCloseGoal : public Goal {
	  public:
		explicit AllayStayCloseGoal(Allay& allay) : _allay(allay) { setFlags({Flag::Move}); }
		bool canUse() override {
			Player* player = _allay.likedPlayer();
			return player && _allay.distanceToSqr(*player) > 16.0 * 16.0;
		}
		bool canContinueToUse() override {
			Player* player = _allay.likedPlayer();
			return player && _allay.distanceToSqr(*player) > 4.0 * 4.0;
		}
		void tick() override {
			if (Player* player = _allay.likedPlayer(); player && (_allay.navigation().isDone() || _allay.tickCount() % 20 == 0)) {
				_allay.navigation().moveTo(*player, 2.25);
			}
		}
		void stop() override { _allay.navigation().stop(); }

	  private:
		Allay& _allay;
	};

	class AllayWanderGoal : public Goal {
	  public:
		explicit AllayWanderGoal(Allay& allay) : _allay(allay) { setFlags({Flag::Move}); }
		bool canUse() override { return _allay.navigation().isDone() && _allay.random().nextInt(20) == 0; }
		bool canContinueToUse() override { return _allay.navigation().isInProgress(); }
		void start() override {
			Vec3				view = viewVectorOf(_allay);
			std::optional<Vec3> pos	 = RandomPos::hoverPos(_allay, 8, 7, view.x, view.z, (float)M_PI / 2.0F, 3, 1);
			if (!pos) pos = RandomPos::airAndWaterPos(_allay, 8, 4, -2, view.x, view.z, M_PI / 2.0);
			if (pos) _allay.navigation().moveTo(pos->x, pos->y, pos->z, 1.0);
		}

	  private:
		Allay& _allay;
	};

	// ----- Copper golem -----

	// CopperGolemAi's TransportItemsBetweenContainers: from a copper chest to the chest that has the same item (or an
	// empty one), in trips of up to 16 items
	class CopperGolemTransportGoal : public Goal {
	  public:
		explicit CopperGolemTransportGoal(CopperGolem& golem) : _golem(golem) { setFlags({Flag::Move, Flag::Look}); }
		bool canUse() override {
			if (_cooldown > 0) {
				_cooldown -= 2;
				return false;
			}
			if (_golem.getMainHandItem().isEmpty()) {
				std::optional<BlockPos> source = findChest(true, ItemStack{});
				if (!source) {
					_cooldown = 140;
					return false;
				}
				_chest = *source, _taking = true;
			} else {
				std::optional<BlockPos> destination = findChest(false, _golem.getMainHandItem());
				if (!destination) destination = _lastSource;
				if (!destination) {
					_cooldown = 140;
					return false;
				}
				_chest = *destination, _taking = false;
			}
			return true;
		}
		bool canContinueToUse() override { return _phase != Phase::Done; }
		void start() override {
			_phase = Phase::Walking;
			_ticks = 0;
			_golem.navigation().moveTo(_chest.x + 0.5, _chest.y, _chest.z + 0.5, 1.0);
		}
		void stop() override {
			_golem.setGolemState(CopperGolem::State::Idle);
			_phase = Phase::Done;
		}
		void tick() override {
			_ticks++;
			if (_phase == Phase::Walking) {
				Vec3 center{_chest.x + 0.5, _chest.y + 0.5, _chest.z + 0.5};
				if ((center - _golem.position()).lengthSqr() <= 2.5 * 2.5) {
					_golem.navigation().stop();
					interact();
					_phase = Phase::Interacting;
					_ticks = 0;
				} else if (_golem.navigation().isDone() && _ticks > 20) {
					_phase	  = Phase::Done;
					_cooldown = 60;
				} else if (_ticks > 600) {
					_phase = Phase::Done;
				}
			} else if (_phase == Phase::Interacting) {
				_golem.lookControl().setLookAt(_chest.x + 0.5, _chest.y + 0.5, _chest.z + 0.5);
				if (_ticks >= CopperGolem::INTERACT_TICKS) {
					_phase	  = Phase::Done;
					_cooldown = _taking ? 0 : 20;
				}
			}
		}

	  private:
		enum class Phase { Walking, Interacting, Done };
		CopperGolem&			_golem;
		Phase					_phase = Phase::Done;
		BlockPos				_chest;
		std::optional<BlockPos> _lastSource;
		bool					_taking	  = true;
		int						_ticks	  = 0, _cooldown = 0;

		Container* containerAt(const BlockPos& pos) { return dynamic_cast<Container*>(_golem.level().getBlockEntity(pos)); }

		void interact() {
			Container* container = containerAt(_chest);
			Level&	   level	 = _golem.level();
			if (!container) return;
			level.playSound(nullptr, _chest, "minecraft:block.copper_chest.open", Level::SoundSource::Blocks, 0.5F, 1.0F);
			if (_taking) {
				for (int slot = 0; slot < container->size(); slot++) {
					ItemStack& stack = container->item(slot);
					if (stack.isEmpty()) continue;
					int		  count = std::min(CopperGolem::MAX_CARRIED, stack.count);
					ItemStack taken = stack.copyWithCount(count);
					stack.shrink(count);
					if (stack.count <= 0) stack = ItemStack{};
					container->setChanged();
					_golem.setItemSlot(EquipmentSlot::MainHand, taken);
					_golem.setGolemState(CopperGolem::State::GettingItem);
					_lastSource = _chest;
					return;
				}
				_golem.setGolemState(CopperGolem::State::GettingNoItem);
				return;
			}
			ItemStack carried = _golem.getMainHandItem();
			int		  max	  = std::min(maxStack(level, carried), container->maxStackSize());
			for (int slot = 0; slot < container->size() && !carried.isEmpty(); slot++) {
				ItemStack& stack = container->item(slot);
				if (!stack.isEmpty() && stack.sameItemSameComponents(carried) && stack.count < max) {
					int moved = std::min(max - stack.count, carried.count);
					stack.grow(moved);
					carried.shrink(moved);
				}
			}
			for (int slot = 0; slot < container->size() && !carried.isEmpty(); slot++) {
				ItemStack& stack = container->item(slot);
				if (stack.isEmpty() && container->canPlaceItem(slot, carried)) {
					stack = carried;
					carried = ItemStack{};
				}
			}
			container->setChanged();
			bool all = carried.isEmpty() || carried.count <= 0;
			_golem.setItemSlot(EquipmentSlot::MainHand, all ? ItemStack{} : carried);
			_golem.setGolemState(all ? CopperGolem::State::DroppingItem : CopperGolem::State::DroppingNoItem);
		}

		std::optional<BlockPos> findChest(bool source, const ItemStack& carried) {
			Level&					level	 = _golem.level();
			BlockPos				center	 = _golem.blockPosition();
			std::optional<BlockPos> empty, matching;
			double					emptyDist = 0.0, matchingDist = 0.0;
			for (int dx = -CopperGolem::SEARCH_RANGE; dx <= CopperGolem::SEARCH_RANGE; dx++) {
				for (int dz = -CopperGolem::SEARCH_RANGE; dz <= CopperGolem::SEARCH_RANGE; dz++) {
					for (int dy = -CopperGolem::SEARCH_HEIGHT; dy <= CopperGolem::SEARCH_HEIGHT; dy++) {
						BlockPos pos = center.offset(dx, dy, dz);
						if (source ? !_golem.isSourceChest(pos) : !_golem.isDestinationChest(pos)) continue;
						Container* container = containerAt(pos);
						if (!container) continue;
						double d		= static_cast<double>(dx) * dx + static_cast<double>(dy) * dy + static_cast<double>(dz) * dz;
						bool   hasAny	= false, hasSame = false, hasRoom = false;
						for (int slot = 0; slot < container->size(); slot++) {
							const ItemStack& stack = container->item(slot);
							if (stack.isEmpty()) {
								hasRoom = true;
								continue;
							}
							hasAny = true;
							if (!source && stack.sameItemSameComponents(carried)) {
								hasSame = true;
								if (stack.count < maxStack(level, stack)) hasRoom = true;
							}
						}
						if (source) {
							if (hasAny && (!matching || d < matchingDist)) matching = pos, matchingDist = d;
						} else if (hasSame && hasRoom) {
							if (!matching || d < matchingDist) matching = pos, matchingDist = d;
						} else if (!hasAny && (!empty || d < emptyDist)) {
							empty = pos, emptyDist = d;
						}
					}
				}
			}
			return matching ? matching : (source ? std::nullopt : empty);
		}
	};

	// ----- Happy ghast -----

	// Its calm drifting: somewhere around, not far above the ground
	class HappyGhastFloatGoal : public Goal {
	  public:
		explicit HappyGhastFloatGoal(HappyGhast& ghast) : _ghast(ghast) { setFlags({Flag::Move}); }
		bool canUse() override { return !_ghast.rider() && _ghast.navigation().isDone() && _ghast.random().nextInt(reducedTickDelay(40)) == 0; }
		bool canContinueToUse() override { return !_ghast.rider() && _ghast.navigation().isInProgress(); }
		void start() override {
			Vec3				view = viewVectorOf(_ghast);
			int					range = _ghast.isBaby() ? 8 : 16;
			std::optional<Vec3> pos	 = RandomPos::hoverPos(_ghast, range, 6, view.x, view.z, (float)M_PI, 6, 2);
			if (pos) _ghast.navigation().moveTo(pos->x, pos->y, pos->z, 1.0);
		}

	  private:
		HappyGhast& _ghast;
	};
} // namespace

// ===================== Allay =====================

Allay::Allay(Level& level, int typeId) : Mob(level, typeId) {
	setMoveControl(std::make_unique<FlyingMoveControl>(*this, 20, true));
	setCanPickUpLoot(false);
}

std::unique_ptr<PathNavigation> Allay::createNavigation() {
	auto navigation = std::make_unique<FlyingPathNavigation>(*this, _level);
	navigation->setCanOpenDoors(false);
	navigation->setCanFloat(true);
	return navigation;
}

void Allay::registerGoals() {
	_goalSelector.addGoal(0, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(1, std::make_unique<AllayGiveGoal>(*this));
	_goalSelector.addGoal(2, std::make_unique<AllayCollectGoal>(*this));
	_goalSelector.addGoal(3, std::make_unique<AllayStayCloseGoal>(*this));
	_goalSelector.addGoal(4, std::make_unique<AllayWanderGoal>(*this));
	_goalSelector.addGoal(5, std::make_unique<LookAtPlayerGoal>(*this, "Player", 6.0F));
}

Player* Allay::likedPlayer() {
	Actor* actor = _liked.isSet() ? _level.actorByRef(_liked) : nullptr;
	Player* player = actor ? actor->asPlayer() : nullptr;
	return player && player->isAlive() && !player->isSpectator() ? player : nullptr;
}

bool Allay::wantsItem(const ItemStack& stack) {
	const ItemStack& held = getMainHandItem();
	if (held.isEmpty() || stack.isEmpty() || stack.item != held.item) return false;
	return _inventory.isEmpty() || (_inventory.sameItemSameComponents(stack) && _inventory.count < maxStack(_level, _inventory));
}

int Allay::addToInventory(const ItemStack& stack) {
	if (!wantsItem(stack)) return 0;
	int max = maxStack(_level, stack);
	if (_inventory.isEmpty()) {
		int taken  = std::min(max, stack.count);
		_inventory = stack.copyWithCount(taken);
		return taken;
	}
	int taken = std::min(max - _inventory.count, stack.count);
	_inventory.grow(taken);
	return taken;
}

bool Allay::mobInteract(Player& player, int hand) {
	ItemStack held = player.getStackInHand(hand);
	if (!hasItemInHand() && !held.isEmpty()) {
		// Allay.mobInteract: it takes one, and likes this player
		setItemSlot(EquipmentSlot::MainHand, held.copyWithCount(1));
		if (!player.isCreative()) player.inventory().getMutable(player.handSlot(hand)).shrink(1);
		_liked = EntityRef(player);
		playSound("minecraft:entity.allay.item_given", 2.0F, 1.0F);
		return true;
	}
	if (hasItemInHand() && hand == 0 && held.isEmpty()) {
		// Taking the item back: it forgets its player and drops what it carried
		giveToPlayer(_level, player, getMainHandItem());
		setItemSlot(EquipmentSlot::MainHand, ItemStack{});
		if (!_inventory.isEmpty()) spawnAtLocation(_inventory);
		_inventory = ItemStack{};
		_liked.clear();
		playSound("minecraft:entity.allay.item_taken", 2.0F, 1.0F);
		return true;
	}
	return Mob::mobInteract(player, hand);
}

bool Allay::hurtServer(const Combat::DamageSource& source, float amount) {
	// Its player can't hurt it
	if (source.causing && source.causing == static_cast<Actor*>(likedPlayer())) return false;
	return Mob::hurtServer(source, amount);
}

void Allay::customServerAiStep() {
	Mob::customServerAiStep();
	if (throwCooldown > 0) throwCooldown--;
	if (_tickCount % 10 == 0) heal(1.0F);
}

void Allay::dropAllDeathLoot(const Combat::DamageSource& source) {
	Mob::dropAllDeathLoot(source);
	if (!_inventory.isEmpty()) spawnAtLocation(_inventory);
	_inventory = ItemStack{};
}

void Allay::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Mob::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_DANCING, onlyNonDefault, true)) writeBoolData(buf, DATA_DANCING, false);
	if (wantsData(mask, DATA_CAN_DUPLICATE, onlyNonDefault, true)) writeBoolData(buf, DATA_CAN_DUPLICATE, true);
}

void Allay::save(Buffer& buf) const {
	Mob::save(buf);
	buf.writeUByte(SAVE_VERSION);
	writeStack(buf, _inventory);
	buf.writeBool(_liked.isSet());
	if (_liked.isSet()) {
		buf.writeLong(static_cast<int64_t>(_liked.uuid.getMostSigBits()));
		buf.writeLong(static_cast<int64_t>(_liked.uuid.getLeastSigBits()));
	}
}

void Allay::load(Buffer& buf) {
	Mob::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown allay data version");
	_inventory = readStack(buf);
	if (buf.readBool()) {
		uint64_t most  = static_cast<uint64_t>(buf.readLong());
		uint64_t least = static_cast<uint64_t>(buf.readLong());
		_liked.uuid	   = UUID(most, least);
		_liked.id	   = -2; // Found by its UUID
	}
}

// ===================== CopperGolem =====================

CopperGolem::CopperGolem(Level& level, int typeId) : Mob(level, typeId) {}

void CopperGolem::registerGoals() {
	_goalSelector.addGoal(0, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(1, std::make_unique<PanicGoal>(*this, 1.5));
	_goalSelector.addGoal(2, std::make_unique<CopperGolemTransportGoal>(*this));
	_goalSelector.addGoal(5, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 1.0));
	_goalSelector.addGoal(6, std::make_unique<LookAtPlayerGoal>(*this, "Player", 6.0F));
	_goalSelector.addGoal(7, std::make_unique<RandomLookAroundGoal>(*this));
}

void CopperGolem::setWeather(Weather weather) {
	if (weather == _weather) return;
	_weather = weather;
	markData(DATA_WEATHER_STATE);
}

void CopperGolem::setGolemState(State state) {
	if (state == _state) return;
	_state = state;
	markData(DATA_COPPER_GOLEM_STATE);
}

bool CopperGolem::isSourceChest(const BlockPos& pos) {
	int block = _level.blocks().blockOf(_level.getBlockState(pos));
	return _level.gameData().isInTag("minecraft:block", "minecraft:copper_chests", block);
}

bool CopperGolem::isDestinationChest(const BlockPos& pos) {
	std::string name = _level.gameData().getStaticName("minecraft:block", _level.blocks().blockOf(_level.getBlockState(pos)));
	return name == "minecraft:chest" || name == "minecraft:trapped_chest";
}

bool CopperGolem::mobInteract(Player& player, int hand) {
	const ItemStack& stack = player.getStackInHand(hand);
	if (isItemIn(_level, stack, "minecraft:axes") && (_waxed || _weather != Weather::Unaffected)) {
		// An axe scrapes off its wax, else a stage of oxidation
		if (_waxed) {
			_waxed = false;
			playSound("minecraft:item.axe.wax_off", 1.0F, 1.0F);
		} else {
			setWeather(static_cast<Weather>(static_cast<int>(_weather) - 1));
			playSound("minecraft:item.axe.scrape", 1.0F, 1.0F);
		}
		_nextWeatheringTick = -1;
		return true;
	}
	if (isItem(_level, stack, "minecraft:honeycomb") && !_waxed) {
		_waxed = true;
		if (!player.isCreative()) player.inventory().getMutable(player.handSlot(hand)).shrink(1);
		playSound("minecraft:item.honeycomb.wax_on", 1.0F, 1.0F);
		return true;
	}
	return Mob::mobInteract(player, hand);
}

void CopperGolem::customServerAiStep() {
	Mob::customServerAiStep();
	// CopperGolem.updateWeathering: a stage every 504000 to 552000 ticks, unless waxed
	if (_waxed || _weather == Weather::Oxidized) return;
	int64_t now = _level.getGameTime();
	if (_nextWeatheringTick < 0) {
		_nextWeatheringTick = now + 504000 + _random.nextInt(48001);
	} else if (now >= _nextWeatheringTick) {
		setWeather(static_cast<Weather>(static_cast<int>(_weather) + 1));
		_nextWeatheringTick = -1;
	}
}

void CopperGolem::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Mob::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_WEATHER_STATE, onlyNonDefault, _weather == Weather::Unaffected)) {
		buf.writeUByte(static_cast<uint8_t>(DATA_WEATHER_STATE));
		buf.writeVarInt(SERIALIZER_WEATHERING_COPPER_STATE);
		buf.writeVarInt(static_cast<int>(_weather));
	}
	if (wantsData(mask, DATA_COPPER_GOLEM_STATE, onlyNonDefault, _state == State::Idle)) {
		buf.writeUByte(static_cast<uint8_t>(DATA_COPPER_GOLEM_STATE));
		buf.writeVarInt(SERIALIZER_COPPER_GOLEM_STATE);
		buf.writeVarInt(static_cast<int>(_state));
	}
}

void CopperGolem::save(Buffer& buf) const {
	Mob::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeUByte(static_cast<uint8_t>(_weather));
	buf.writeBool(_waxed);
	buf.writeLong(_nextWeatheringTick);
}

void CopperGolem::load(Buffer& buf) {
	Mob::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown copper golem data version");
	setWeather(static_cast<Weather>(buf.readUByte() & 3));
	_waxed				= buf.readBool();
	_nextWeatheringTick = buf.readLong();
}

// ===================== HappyGhast =====================

HappyGhast::HappyGhast(Level& level, int typeId) : Animal(level, typeId) {
	setMoveControl(std::make_unique<FlyingMoveControl>(*this, 10, true));
	setNoGravity(true);
}

std::unique_ptr<PathNavigation> HappyGhast::createNavigation() {
	auto navigation = std::make_unique<FlyingPathNavigation>(*this, _level);
	navigation->setCanOpenDoors(false);
	navigation->setCanFloat(true);
	return navigation;
}

void HappyGhast::registerGoals() {
	_goalSelector.addGoal(1, std::make_unique<TemptGoal>(
								 *this, 1.0,
								 [this](const ItemStack& stack) {
									 return isItemIn(_level, stack, "minecraft:happy_ghast_tempt_items") || (isBaby() && isFood(stack));
								 },
								 false, 4.0));
	_goalSelector.addGoal(3, std::make_unique<HappyGhastFloatGoal>(*this));
	_goalSelector.addGoal(4, std::make_unique<LookAtPlayerGoal>(*this, "Player", 8.0F));
}

bool HappyGhast::isFood(const ItemStack& stack) const { return isItemIn(_level, stack, "minecraft:happy_ghast_food"); }

bool HappyGhast::isHarnessed() const { return !_equipment[static_cast<int>(EquipmentSlot::Body)].isEmpty(); }

bool HappyGhast::mobInteract(Player& player, int hand) {
	const ItemStack& stack = player.getStackInHand(hand);
	if (isBaby()) return Animal::mobInteract(player, hand); // Snowballs make it grow
	if (!isHarnessed() && !stack.isEmpty() && equipFromPlayer(player, hand, EquipmentSlot::Body)) {
		playSound("minecraft:entity.happy_ghast.harness_goggles_down", 1.0F, 1.0F);
		return true;
	}
	if (isHarnessed() && canAddPassenger() && !player.isShiftKeyDown()) {
		navigation().stop();
		startRiding(player);
		return true;
	}
	return Animal::mobInteract(player, hand);
}

void HappyGhast::travel(const Vec3& input) {
	// It floats: no gravity, a gentle drag
	setNoGravity(true);
	travelFlying(input, static_cast<float>(getAttributeValue(_ids.flyingSpeed)) * 0.5F);
}
