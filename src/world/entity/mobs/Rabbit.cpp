#include "world/entity/mobs/Rabbit.hpp"

#include "world/Level.hpp"
#include "world/entity/ai/Controls.hpp"
#include "world/entity/ai/Goals.hpp"

#include <functional>

namespace {
	// RabbitJumpControl: the rabbit hops again as soon as it lands (RabbitEntity)
	class RabbitJumpControl : public JumpControl {
	  public:
		explicit RabbitJumpControl(Rabbit& rabbit) : JumpControl(rabbit), _rabbit(rabbit) {}
		void tick() override {
			bool onGround = _rabbit.onGround();
			if (onGround && !_wasOnGround) jump();
			_wasOnGround = onGround;
			JumpControl::tick();
		}

	  private:
		Rabbit& _rabbit;
		bool	_wasOnGround = true;
	};
	std::function<bool(const ItemStack&)> itemTag(Level& level, const std::string& tag) {
		return [&level, tag](const ItemStack& stack) { return !stack.isEmpty() && level.gameData().isInTag("minecraft:item", tag, stack.item); };
	}
} // namespace

Rabbit::Rabbit(Level& level, int typeId) : Animal(level, typeId) {
	setJumpControl(std::make_unique<RabbitJumpControl>(*this));
}

void Rabbit::registerGoals() {
	_goalSelector.addGoal(1, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(2, std::make_unique<AvoidEntityGoal>(*this, "Player", 16.0F, 1.33, 1.8));
	_goalSelector.addGoal(2, std::make_unique<AvoidEntityGoal>(*this, "Wolf", 10.0F, 1.33, 1.8));
	_goalSelector.addGoal(2, std::make_unique<AvoidEntityGoal>(*this, "Fox", 10.0F, 1.33, 1.8));
	_goalSelector.addGoal(3, std::make_unique<PanicGoal>(*this, 2.2));
	_goalSelector.addGoal(4, std::make_unique<BreedGoal>(*this, 1.0));
	_goalSelector.addGoal(5, std::make_unique<TemptGoal>(*this, 1.0, itemTag(_level, "minecraft:rabbit_food"), false));
	_goalSelector.addGoal(6, std::make_unique<FollowParentGoal>(*this, 1.1));
	_goalSelector.addGoal(7, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 1.0));
	_goalSelector.addGoal(8, std::make_unique<LookAtPlayerGoal>(*this, "Player", 8.0F));
	_goalSelector.addGoal(9, std::make_unique<RandomLookAroundGoal>(*this));
}