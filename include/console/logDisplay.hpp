#pragma once
#include "console/base.hpp"
#include "console/event.hpp"
#include "console/terminal.hpp"
#include "logger.hpp"

#include <cstddef>
#include <deque>
#include <mutex>
#include <string>

class LogDisplayWidget : public Base {
  private:
	std::deque<LogEntry> _displayLogs;
	size_t _maxDisplayLines;
	int _scrollOffset;
	std::mutex _displayMutex;
	bool _autoScroll;

	// Visual settings
	std::string _title;
	bool _showBorder;
	std::string _levelColors[4]; // Colors for each log level

  public:
	LogDisplayWidget(Position pos, Size size, const std::string& title = "Game Logs");
	~LogDisplayWidget();

	void draw(Term& term) override;
	bool handleEvent(const Event& event) override; // For scrolling

	// Called by LogManager
	void addLogEntry(const LogEntry& entry);

	// Configuration
	void setMaxLines(size_t maxLines) {
		_maxDisplayLines = maxLines;
	}
	void setAutoScroll(bool autoScroll) {
		_autoScroll = autoScroll;
	}
	void setShowBorder(bool show) {
		_showBorder = show;
	}

	// Filtering (future enhancement)
	void setLevelFilter(LogLevel minLevel);

  private:
	std::string formatLogForDisplay(const LogEntry& entry);
	void trimOldLogs();
};
