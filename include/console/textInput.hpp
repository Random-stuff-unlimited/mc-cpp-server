#pragma once
#include "console/base.hpp"
#include "console/event.hpp"
#include "console/terminal.hpp"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <string>

class TextInput : public Base {
  private:
	std::string input_text;
	size_t cursor_pos = 0;
	size_t max_length = 100;
	char mask_char    = 0; // 0 = no masking, '*' for password fields

  public:
	TextInput(Position pos = {0, 0}, int width = 20) : Base(pos, {width, 1}) {
		_focusable = true;
		fgColor    = Color::BLACK;
		bgColor    = BGColor::WHITE;
	}

	void draw(Term& term) override {
		if (!_visible)
			return;

		Color input_fg   = _hasFocus ? Color::BLACK : Color::WHITE;
		BGColor input_bg = _hasFocus ? BGColor::WHITE : BGColor::BLACK;

		term.moveCursor(_position.x, _position.y);
		term.setColor(input_fg, input_bg);

		std::string display = input_text;
		if (mask_char != 0) {
			display = std::string(input_text.length(), mask_char);
		}

		// Pad or truncate to fit width
		if (static_cast<int>(display.length()) < _size.width) {
			display += std::string(_size.width - static_cast<int>(display.length()), ' ');
		} else if (static_cast<int>(display.length()) > _size.width) {
			display = display.substr(0, _size.width);
		}

		std::cout << display;

		// Show cursor if focused
		if (_hasFocus) {
			term.moveCursor(_position.x +
			                        std::min(cursor_pos, static_cast<size_t>(_size.width - 1)),
			                _position.y);
		}

		term.resetColor();
	}

	bool handleEvent(const Event& event) override {
		if (event.type != EventType::KEY_PRESS)
			return false;

		switch (event.key) {
		case '\b':
		case 127: // Backspace
			if (cursor_pos > 0) {
				input_text.erase(cursor_pos - 1, 1);
				cursor_pos--;
			}
			return true;

		case '\n':
		case '\r': // Enter
			// Could trigger a submit event here
			return true;

		default:
			if (event.key >= 32 && event.key < 127 && input_text.length() < max_length) {
				input_text.insert(cursor_pos, 1, event.key);
				cursor_pos++;
			}
			return true;
		}
	}

	std::string getText() const {
		return input_text;
	}
	void setText(const std::string& text) {
		input_text = text;
		cursor_pos = std::min(cursor_pos, input_text.length());
	}

	void setMaskChar(char c) {
		mask_char = c;
	}
	void setMaxLength(size_t len) {
		max_length = len;
	}
};
