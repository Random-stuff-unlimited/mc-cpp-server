#pragma once
#include "console/base.hpp"
#include "console/event.hpp"
#include "console/terminal.hpp"

#include <functional>
#include <iostream>
#include <string>

class Button : public Base {
  private:
	std::function<void()> click_handler;
	std::string button_text;

  public:
	Button(const std::string& text, Position pos = {0, 0}, std::function<void()> handler = nullptr)
	    : Base(pos, {static_cast<int>(text.length() + 4), 3}), click_handler(handler),
	      button_text(text) {
		_focusable = true;
		fgColor    = Color::WHITE;
		bgColor    = BGColor::BLUE;
	}

	void draw(Term& term) override {
		if (!_visible)
			return;

		Color button_fg   = _hasFocus ? Color::BRIGHT_YELLOW : fgColor;
		BGColor button_bg = _hasFocus ? BGColor::BRIGHT_BLUE : bgColor;

		// Top border
		term.moveCursor(_position.x, _position.y);
		term.setColor(button_fg, button_bg);
		std::cout << "┌";
		for (int i = 0; i < _size.width - 2; ++i)
			std::cout << "─";
		std::cout << "┐";

		// Middle with text
		term.moveCursor(_position.x, _position.y + 1);
		std::cout << "│ " << button_text << " │";

		// Bottom border
		term.moveCursor(_position.x, _position.y + 2);
		std::cout << "└";
		for (int i = 0; i < _size.width - 2; ++i)
			std::cout << "─";
		std::cout << "┘";

		term.resetColor();
	}

	bool handleEvent(const Event& event) override {
		if (event.type == EventType::KEY_PRESS &&
		    (event.key == '\n' || event.key == '\r' || event.key == ' ')) {
			if (click_handler) {
				click_handler();
			}
			return true;
		}
		return false;
	}

	void setClickHandler(std::function<void()> handler) {
		click_handler = handler;
	}
};
