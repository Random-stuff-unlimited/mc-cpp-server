#pragma once

#include "console/base.hpp"
#include "console/border.hpp"
#include "console/button.hpp"
#include "console/container.hpp"
#include "console/label.hpp"
#include "console/terminal.hpp"
#include "console/textInput.hpp"

#include <chrono>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <utility>

class Console {
  private:
	Term _terminal;
	std::unique_ptr<Container> _rootContainer;
	bool _running = false;
	Size screenSize;

  public:
	Console() {
		_rootContainer = std::make_unique<Container>();
		screenSize     = _terminal.getTerminalSize();
		_rootContainer->setSize(screenSize);
	}

	~Console() {
		stop();
	}

	void addBase(std::unique_ptr<Base> base) {
		_rootContainer->addChild(std::move(base));
	}

	void run() {
		if (_running)
			return;

		_running = true;
		_terminal.enableRawMode();
		_terminal.hideCursor();
		_terminal.clearScreen();

		while (_running) {
			handleEvents();
			draw();

			std::this_thread::sleep_for(std::chrono::milliseconds(16));
		}
		cleanup();
	}

  private:
	void handleEvents() {
		char key = _terminal.readKey();
		if (key == 0)
			return;

		// Global quit key (Ctrl+C or ESC)
		if (key == 3 || key == 27) { // Ctrl+C or ESC
			stop();
			return;
		}

		// Create event and pass to widgets
		Event event(EventType::KEY_PRESS, key);
		_rootContainer->handleEvent(event);
	}

	void draw() {
		// Check if terminal size changed
		Size new_size = _terminal.getTerminalSize();
		if (new_size.width != screenSize.width || new_size.height != screenSize.height) {
			screenSize = new_size;
			_rootContainer->setSize(screenSize);
			_terminal.clearScreen();
		}

		// Move to top-left and draw everything
		_terminal.moveCursor(0, 0);
		_rootContainer->draw(_terminal);
		_terminal.flush();
	}

	void cleanup() {
		_terminal.clearScreen();
		_terminal.showCursor();
		_terminal.resetColor();
		_terminal.moveCursor(0, 0);
		_terminal.disableRawMode();
	}

  public:
	void stop() {
		_running = false;
	}
};

// Utility functions for creating widgets

inline std::unique_ptr<Label> make_label(const std::string& text, int x, int y) {
	return std::make_unique<Label>(text, Position{x, y});
}

inline std::unique_ptr<Button>
make_button(const std::string& text, int x, int y, std::function<void()> handler = nullptr) {
	return std::make_unique<Button>(text, Position{x, y}, handler);
}

inline std::unique_ptr<TextInput> make_text_input(int x, int y, int width = 20) {
	return std::make_unique<TextInput>(Position{x, y}, width);
}

inline std::unique_ptr<Border>
make_border(int x, int y, int w, int h, const std::string& title = "") {
	return std::make_unique<Border>(Size{w, h}, Position{x, y}, title);
}
