#include "TickLoop.hpp"

#include "logger.hpp"
#include "network/networking.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"

#include <algorithm>
#include <exception>
#include <string>

using Clock = std::chrono::steady_clock;

namespace {
	constexpr auto OVERLOAD_WARNING_INTERVAL = std::chrono::seconds(15);
	// Between ticks, output is also sent at this interval (matters at low tick rates only)
	constexpr auto IDLE_FLUSH_INTERVAL = std::chrono::milliseconds(50);
} // namespace

TickLoop::TickLoop(Server& server) : _server(server), _gameThread(std::thread::id()) {}

void TickLoop::run(const std::function<bool()>& shouldStop) {
	_gameThread = std::this_thread::get_id();
	Clock::time_point next = Clock::now();

	while (!shouldStop()) {
		Clock::time_point now = Clock::now();
		if (now < next) {
			runTasks(next);
			continue;
		}

		if (_sprintLeft > 0) {
			next = now; // As fast as possible
		} else if (now - next > std::chrono::milliseconds(MAX_BEHIND_MS)) {
			int64_t behindMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - next).count();
			if (now - _lastOverloadWarning >= OVERLOAD_WARNING_INTERVAL) {
				_lastOverloadWarning = now;
				g_logger->logGameInfo(WARN,
									  "Can't keep up! Is the server overloaded? Running " + std::to_string(behindMs) + "ms or " +
											  std::to_string(behindMs * 1000000 / tickInterval().count()) + " ticks behind",
									  "Server");
			}
			next = now; // Skip the late ticks instead of running them all at once
		}

		tick();
		_server.getNetworkManager().flush();
		Clock::time_point end = Clock::now();
		_msptNanos[_tickCount % MSPT_SAMPLES] = std::chrono::duration_cast<std::chrono::nanoseconds>(end - now).count();
		next += tickInterval();
		runTasks(end); // What arrived during the tick, without waiting
	}

	// Whatever the last tasks sent
	_server.getNetworkManager().flush();
}

void TickLoop::tick() {
	_tickCount++;
	_server.tick(isRunningNormally());
	if (_stepsLeft > 0) _stepsLeft--;
	if (_sprintLeft > 0 && --_sprintLeft == 0) g_logger->logGameInfo(INFO, "Sprint completed", "Server");
}

void TickLoop::runTasks(Clock::time_point until) {
	Clock::time_point lastFlush = Clock::now();
	while (true) {
		{
			std::unique_lock<std::mutex> lock(_inboxMutex);
			if (_inbox.empty() && !_inboxCondition.wait_until(lock, until, [this] { return !_inbox.empty(); })) return;
			_running.swap(_inbox);
		}
		for (Task& task : _running) runTask(task);
		_running.clear();

		Clock::time_point now = Clock::now();
		if (now - lastFlush >= IDLE_FLUSH_INTERVAL) {
			_server.getNetworkManager().flush();
			lastFlush = now;
		}
		if (now >= until) return;
	}
}

void TickLoop::runTask(Task& task) {
	try {
		if (task.packet) {
			_server.handleGamePacket(task.packet);
		} else {
			task.job();
		}
	} catch (const std::exception& e) {
		g_logger->logGameInfo(ERROR, "Error in a game thread task: " + std::string(e.what()), "Server");
	}
}

void TickLoop::post(Packet* packet) {
	bool wake;
	{
		std::lock_guard<std::mutex> lock(_inboxMutex);
		wake = _inbox.empty();
		_inbox.push_back({packet, nullptr});
	}
	if (wake) _inboxCondition.notify_one();
}

void TickLoop::post(std::function<void()> job) {
	bool wake;
	{
		std::lock_guard<std::mutex> lock(_inboxMutex);
		wake = _inbox.empty();
		_inbox.push_back({nullptr, std::move(job)});
	}
	if (wake) _inboxCondition.notify_one();
}

void TickLoop::runOnGameThread(std::function<void()> job) {
	if (isGameThread()) {
		job();
	} else {
		post(std::move(job));
	}
}

std::chrono::nanoseconds TickLoop::tickInterval() const { return std::chrono::nanoseconds(static_cast<int64_t>(1e9 / _tickRate)); }

void TickLoop::setTickRate(float rate) {
	_tickRate = std::clamp(rate, MIN_TICK_RATE, MAX_TICK_RATE);
	_server.sendTickingState(nullptr);
}

void TickLoop::setFrozen(bool frozen) {
	_frozen	   = frozen;
	_stepsLeft = 0;
	_server.sendTickingState(nullptr);
}

bool TickLoop::step(int ticks) {
	if (!_frozen || ticks <= 0) return false;
	_stepsLeft = ticks;
	_server.sendTickingState(nullptr);
	return true;
}

void TickLoop::sprint(int ticks) { _sprintLeft = std::max(0, ticks); }

double TickLoop::getAverageMspt() const {
	int		count = static_cast<int>(std::min<int64_t>(_tickCount, MSPT_SAMPLES));
	int64_t total = 0;
	for (int i = 0; i < count; i++) total += _msptNanos[i];
	return count == 0 ? 0.0 : total / 1e6 / count;
}
