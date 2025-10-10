#include "lib/filesystem.hpp"
#include "logger.hpp"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <thread>

namespace fs = std::filesystem;

LogManager::LogManager() : _running(false) {
	if (initializeLogDirectory()) {
		_running	  = true;
		_writerThread = std::thread(&LogManager::writerThreadLoop, this);
		// Logger initialized - using console for initialization message
		std::cout << "Logger initialized successfully in directory: " << _logDir << std::endl;
	} else {
		std::cerr << "Failed to initialize logger!" << std::endl;
	}
}

LogManager::~LogManager() {
	if (_running) {
		_running = false;
		if (_writerThread.joinable()) {
			_writerThread.join();
		}
	}

	if (_networkFile.is_open()) {
		_networkFile.close();
	}
	if (_gameInfoFile.is_open()) {
		_gameInfoFile.close();
	}
}

std::string LogManager::getCurrentTimestamp() {
	auto now	= std::chrono::system_clock::now();
	auto time_t = std::chrono::system_clock::to_time_t(now);

	std::stringstream ss;
	ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d_%H-%M-%S");
	return ss.str();
}

std::string LogManager::getDetailedTimestamp() {
	auto now	= std::chrono::system_clock::now();
	auto time_t = std::chrono::system_clock::to_time_t(now);
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

	std::stringstream ss;
	ss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
	ss << "." << std::setfill('0') << std::setw(3) << ms.count();
	return ss.str();
}

bool LogManager::initializeLogDirectory() {
	std::string timestamp = getCurrentTimestamp();

	// Create base logs directory
	fs::path baseLogsDir = getPath().parent_path() / "logs";
	if (!fs::exists(baseLogsDir)) {
		if (!fs::create_directories(baseLogsDir)) {
			std::cerr << "Failed to create base logs directory: " << baseLogsDir << std::endl;
			return false;
		}
	}

	// Create session-specific directory with timestamp
	_logDir = baseLogsDir / timestamp;
	if (!fs::exists(_logDir)) {
		if (!fs::create_directories(_logDir)) {
			std::cerr << "Failed to create session log directory: " << _logDir << std::endl;
			return false;
		}
	}

	// Create log files with timestamp in the directory
	fs::path networkFile  = _logDir / ("network-" + timestamp + ".txt");
	fs::path gameinfoFile = _logDir / ("gameinfo-" + timestamp + ".txt");

	_networkFile.open(networkFile, std::ios::app);
	_gameInfoFile.open(gameinfoFile, std::ios::app);

	if (!_networkFile.is_open()) {
		std::cerr << "Failed to open network log file: " << networkFile << std::endl;
		return false;
	}

	if (!_gameInfoFile.is_open()) {
		std::cerr << "Failed to open gameinfo log file: " << gameinfoFile << std::endl;
		return false;
	}

	// Write initial headers
	_networkFile << "=== Network Log Session Started at " << timestamp << " ===" << std::endl;
	_gameInfoFile << "=== Game Info Log Session Started at " << timestamp << " ===" << std::endl;
	_networkFile.flush();
	_gameInfoFile.flush();

	return true;
}

void LogManager::log(LogLevel			level,
					 LogCategory		category,
					 const std::string& message,
					 const std::string& source) {
	LogEntry entry;
	entry.timestamp = std::chrono::system_clock::now();
	entry.level		= level;
	entry.category	= category;
	entry.message	= message;
	entry.source	= source;

	// Add to queue for file writing
	{
		std::lock_guard<std::mutex> lock(_queueMutex);
		_logQueue.push(entry);
	}

	if (category == GAMEINFO ||
		category == NETWORK) { // Remove category == NETWORK on release build
		std::string formattedEntry = formatLogEntry(entry);
		std::cout << formattedEntry << std::endl;
	}
}

void LogManager::logNetwork(LogLevel level, const std::string& message, const std::string& source) {
	log(level, NETWORK, message, source);
}

void LogManager::logGameInfo(LogLevel			level,
							 const std::string& message,
							 const std::string& source) {
	log(level, GAMEINFO, message, source);
}

void LogManager::writerThreadLoop() {
	while (_running || !_logQueue.empty()) {
		LogEntry entry;
		bool	 hasEntry = false;

		// Get entry from queue
		{
			std::lock_guard<std::mutex> lock(_queueMutex);
			if (!_logQueue.empty()) {
				entry = _logQueue.front();
				_logQueue.pop();
				hasEntry = true;
			}
		}

		if (hasEntry) {
			// Write to appropriate file
			std::lock_guard<std::mutex> fileLock(_fileMutex);
			std::string					formattedEntry = formatLogEntry(entry);

			if (entry.category == NETWORK && _networkFile.is_open()) {
				_networkFile << formattedEntry << std::endl;
				_networkFile.flush();
			} else if (entry.category == GAMEINFO && _gameInfoFile.is_open()) {
				_gameInfoFile << formattedEntry << std::endl;
				_gameInfoFile.flush();
			}
		} else {
			// Sleep briefly if no entries to process
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}
}

std::string LogManager::formatLogEntry(const LogEntry& entry) {
	std::stringstream ss;

	// Add timestamp
	auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
	auto ms		= std::chrono::duration_cast<std::chrono::milliseconds>(
					  entry.timestamp.time_since_epoch()) %
			  1000;

	ss << "[" << std::put_time(std::localtime(&time_t), "%H:%M:%S");
	ss << "." << std::setfill('0') << std::setw(3) << ms.count() << "] ";

	// Add log level
	switch (entry.level) {
	case DEBUG:
		ss << "[DEBUG] ";
		break;
	case INFO:
		ss << "[INFO] ";
		break;
	case WARN:
		ss << "[WARN] ";
		break;
	case ERROR:
		ss << "[ERROR] ";
		break;
	}

	// Add source if provided
	if (!entry.source.empty()) {
		ss << "[" << entry.source << "] ";
	}

	// Add message
	ss << entry.message;

	return ss.str();
}

// Global logger instance
std::unique_ptr<LogManager> g_logger = nullptr;

void initializeGlobalLogger() {
	if (!g_logger) {
		g_logger = std::make_unique<LogManager>();
	}
}
