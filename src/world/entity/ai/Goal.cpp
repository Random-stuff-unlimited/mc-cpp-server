#include "world/entity/ai/Goal.hpp"

#include <algorithm>

void WrappedGoal::start() {
	if (_running) return;
	_running = true;
	_goal->start();
}

void WrappedGoal::stop() {
	if (!_running) return;
	_running = false;
	_goal->stop();
}

void GoalSelector::removeGoal(const Goal& goal) {
	for (auto& boxed : _goals) {
		WrappedGoal& wrapped = *boxed;
		if (&wrapped.goal() == &goal && wrapped.isRunning()) wrapped.stop();
	}
	for (WrappedGoal*& locked : _locked) {
		if (locked && &locked->goal() == &goal) locked = nullptr;
	}
	_goals.erase(std::remove_if(_goals.begin(), _goals.end(), [&](const std::unique_ptr<WrappedGoal>& w) { return &w->goal() == &goal; }), _goals.end());
}

// GoalSelector.tick: goalCleanup, then goalUpdate, then goalTick
void GoalSelector::tick() {
	if (_goals.empty()) return; // Nothing can run: the common case until mobs get their AI
	for (auto& boxed : _goals) {
		WrappedGoal& wrapped = *boxed;
		if (wrapped.isRunning() && ((wrapped.goal().flags() & _disabledFlags) || !wrapped.goal().canContinueToUse())) wrapped.stop();
	}
	for (WrappedGoal*& locked : _locked) {
		if (locked && !locked->isRunning()) locked = nullptr;
	}
	for (auto& boxed : _goals) {
		WrappedGoal& wrapped = *boxed;
		uint8_t flags = wrapped.goal().flags();
		if (wrapped.isRunning() || (flags & _disabledFlags)) continue;
		// goalCanBeReplacedForAllFlags: every flag it needs is free or held by a goal it can replace
		bool replaceable = true;
		for (int flag = 0; flag < Goal::FLAG_COUNT && replaceable; flag++) {
			if (!(flags & (1u << flag))) continue;
			WrappedGoal* holder = _locked[flag];
			replaceable			= holder ? holder->canBeReplacedBy(wrapped) : true; // NO_GOAL: priority MAX_VALUE, interruptable
		}
		if (!replaceable || !wrapped.goal().canUse()) continue;
		for (int flag = 0; flag < Goal::FLAG_COUNT; flag++) {
			if (!(flags & (1u << flag))) continue;
			if (_locked[flag]) _locked[flag]->stop();
			_locked[flag] = &wrapped;
		}
		wrapped.start();
	}
	tickRunningGoals(true);
}

void GoalSelector::tickRunningGoals(bool all) {
	for (auto& boxed : _goals) {
		WrappedGoal& wrapped = *boxed;
		if (wrapped.isRunning() && (all || wrapped.goal().requiresUpdateEveryTick())) wrapped.goal().tick();
	}
}

void GoalSelector::setControlFlag(Goal::Flag flag, bool enabled) {
	if (enabled) {
		_disabledFlags &= static_cast<uint8_t>(~Goal::bit(flag));
	} else {
		_disabledFlags |= Goal::bit(flag);
	}
}
