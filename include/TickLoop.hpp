#ifndef TICK_LOOP_HPP
#define TICK_LOOP_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

class Packet;
class Server;

// The game thread: the only thread that touches the game state (players in game, world simulation, tracker...).
//
// Like vanilla, it runs fixed-length ticks (50 ms at 20 TPS) and, between two ticks, handles the work posted by the
// other threads as soon as it arrives: Play packets from the network threads, chunks ready from the I/O threads.
// What the game sends during a tick is written to the sockets once, at the end of the tick.
//
// All game durations are counted in ticks, so changing the tick rate speeds the whole game up or down like
// vanilla's /tick rate. Only network timeouts (keep-alive) and saving stay in real time.
class TickLoop {
  public:
	static constexpr float MIN_TICK_RATE = 1.0f;
	static constexpr float MAX_TICK_RATE = 10000.0f;

	explicit TickLoop(Server& server);

	// Runs on the calling thread until shouldStop() returns true (checked at least every tick)
	void run(const std::function<bool()>& shouldStop);

	// Any thread. The packet is deleted after it is handled
	void post(Packet* packet);
	void post(std::function<void()> job);
	// Runs the job right away when called on the game thread, posts it otherwise
	void runOnGameThread(std::function<void()> job);
	bool isGameThread() const { return std::this_thread::get_id() == _gameThread.load(); }

	// Game thread only (like the /tick command)
	float getTickRate() const { return _tickRate; }
	void  setTickRate(float rate);
	bool  isFrozen() const { return _frozen; }
	void  setFrozen(bool frozen);
	// While frozen: runs this many ticks normally, then freezes again. Returns false if not frozen
	bool step(int ticks);
	// Runs this many ticks as fast as possible, then goes back to the tick rate
	void sprint(int ticks);
	int	 getStepsLeft() const { return _stepsLeft; }
	// False while frozen (and not stepping): the world doesn't advance, players still do
	bool isRunningNormally() const { return !_frozen || _stepsLeft > 0; }

	// Ticks run since the server started, frozen ones included (player timers)
	int64_t getTickCount() const { return _tickCount; }
	// Average milliseconds per tick over the last 100 ticks
	double getAverageMspt() const;

  private:
	struct Task {
		Packet*				  packet = nullptr;
		std::function<void()> job;
	};
	static constexpr int	 MSPT_SAMPLES = 100;
	static constexpr int64_t MAX_BEHIND_MS = 2000; // Further behind, ticks are skipped instead of caught up

	Server&					 _server;
	std::atomic<std::thread::id> _gameThread;

	std::mutex				_inboxMutex;
	std::condition_variable _inboxCondition;
	std::vector<Task>		_inbox;
	std::vector<Task>		_running; // Tasks being run, swapped with _inbox (keeps both capacities)

	float	_tickRate  = 20.0f;
	bool	_frozen	   = false;
	int		_stepsLeft = 0;
	int		_sprintLeft = 0;
	int64_t _tickCount = 0;

	std::chrono::steady_clock::time_point _lastOverloadWarning;
	int64_t								  _msptNanos[MSPT_SAMPLES] = {};

	std::chrono::nanoseconds tickInterval() const;
	// Runs the posted tasks until the inbox is empty or `until` is reached
	void runTasks(std::chrono::steady_clock::time_point until);
	void runTask(Task& task);
	void tick();
};

#endif
