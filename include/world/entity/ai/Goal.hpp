#ifndef GOAL_HPP
#define GOAL_HPP

#include <array>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <vector>

// Goal-based AI (vanilla's net.minecraft.world.entity.ai.goal): a mob's goal selectors pick, every other tick, the
// goals that can run by priority (lower first) and by the controls they need (flags), then tick the running ones.
//
// Writing a goal: subclass Goal, set its flags in the constructor (setFlags({Goal::Flag::Move, Goal::Flag::Look})),
// implement canUse() (and canContinueToUse, start, stop, tick as needed), then register it for the mob types that
// use it in src/world/entity/ai/MobGoals.cpp (see MobRegistry). Goals steer the mob through its controls
// (mob.moveControl(), lookControl(), jumpControl()) and its navigation, never by moving it directly.
class Goal {
  public:
	// Goal.Flag: the controls a goal takes; two goals with a common flag can't run together
	enum class Flag : uint8_t { Move = 0, Look = 1, Jump = 2, Target = 3 };
	static constexpr int FLAG_COUNT = 4;

	virtual ~Goal() = default;

	virtual bool canUse() = 0;
	virtual bool canContinueToUse() { return canUse(); }
	virtual bool isInterruptable() { return true; }
	virtual void start() {}
	virtual void stop() {}
	// Ticked every tick instead of every other tick (when the selector only ticks the running goals)
	virtual bool requiresUpdateEveryTick() { return false; }
	virtual void tick() {}

	void	setFlags(std::initializer_list<Flag> flags) {
		   _flags = 0;
		   for (Flag flag : flags) _flags |= bit(flag);
	}
	uint8_t flags() const { return _flags; }
	static uint8_t bit(Flag flag) { return static_cast<uint8_t>(1u << static_cast<int>(flag)); }

  protected:
	// Goal.adjustedTickDelay / reducedTickDelay: delays in ticks, halved for goals ticked every other tick
	int		   adjustedTickDelay(int ticks) { return requiresUpdateEveryTick() ? ticks : reducedTickDelay(ticks); }
	static int reducedTickDelay(int ticks) { return (ticks + 1) / 2; } // Mth.positiveCeilDiv(ticks, 2)

  private:
	uint8_t _flags = 0;
};

// WrappedGoal: a goal with its priority in a selector, and whether it runs
class WrappedGoal {
  public:
	WrappedGoal(int priority, std::unique_ptr<Goal> goal) : _goal(std::move(goal)), _priority(priority) {}

	bool  canBeReplacedBy(const WrappedGoal& other) const { return _goal->isInterruptable() && other._priority < _priority; }
	void  start();
	void  stop();
	bool  isRunning() const { return _running; }
	int	  priority() const { return _priority; }
	Goal& goal() const { return *_goal; }

  private:
	std::unique_ptr<Goal> _goal;
	int					  _priority;
	bool				  _running = false;
};

// GoalSelector: the goals in the order they were added (vanilla's linked set)
class GoalSelector {
  public:
	void addGoal(int priority, std::unique_ptr<Goal> goal) { _goals.push_back(std::make_unique<WrappedGoal>(priority, std::move(goal))); }
	// Stopped first if it runs
	void removeGoal(const Goal& goal);
	void removeAllGoals() { _goals.clear(); _locked.fill(nullptr); }
	bool empty() const { return _goals.empty(); }
	// Stops the goals that can't continue, starts the ones that can, then ticks the running ones
	void tick();
	// Ticks the running goals only (all of them, or those that need every tick)
	void tickRunningGoals(bool all);
	void setControlFlag(Goal::Flag flag, bool enabled);
	const std::vector<std::unique_ptr<WrappedGoal>>& goals() const { return _goals; }

  private:
	std::vector<std::unique_ptr<WrappedGoal>>	 _goals; // Boxed: the locks point to them
	std::array<WrappedGoal*, Goal::FLAG_COUNT>	 _locked{}; // Goal holding each flag (lockedFlags), null if none
	uint8_t										 _disabledFlags = 0;
};

#endif
