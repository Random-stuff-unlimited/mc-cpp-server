#pragma once
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

enum LogLevel { DEBUG, INFO, WARN, ERROR };
enum LogCategory { NETWORK, GAMEINFO };

struct LogEntry {
	std::chrono::system_clock::time_point timestamp;
	LogLevel level;
	LogCategory category;
	std::string message;
	std::string source;
};

class LogManager {
  private:
	std::filesystem::path _logDir;
	std::ofstream _networkFile;
	std::ofstream _gameInfoFile;
	std::queue<LogEntry> _logQueue;
	std::mutex _queueMutex;
	std::mutex _fileMutex;
	std::thread _writerThread;
	bool _running;

	// TUI integration
	std::vector<std::function<void(const LogEntry&)>> _tuiCallbacks;
	std::mutex _callbackMutex;

	// Display settings (for future TUI integration)
	int _scrollOffset;
	bool _autoScroll;

  public:
	LogManager();
	~LogManager();

	// Core logging methods
	void log(LogLevel level,
	         LogCategory category,
	         const std::string& message,
	         const std::string& source = "");
	void logNetwork(LogLevel level, const std::string& message, const std::string& source = "");
	void logGameInfo(LogLevel level, const std::string& message, const std::string& source = "");

	// TUI integration
	void registerTUICallback(std::function<void(const LogEntry&)> callback);
	void unregisterTUICallback();

  private:
	bool initializeLogDirectory();
	void writerThreadLoop();
	std::string formatLogEntry(const LogEntry& entry);
	std::string getCurrentTimestamp();
	std::string getDetailedTimestamp();
};

// Global logger instance
extern std::unique_ptr<LogManager> g_logger;
void initializeGlobalLogger();
