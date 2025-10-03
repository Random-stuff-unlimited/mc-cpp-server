#pragma once
#include "console/base.hpp"
#include "console/terminal.hpp"

#include <algorithm>
#include <iostream>
#include <string>

class Label : public Base {
  private:
	std::string display_text;

  public:
	Label(const std::string& text, Position pos = {0, 0})
	    : Base(pos, {static_cast<int>(text.length()), 1}), display_text(text) {
		this->text = text;
	}

	void draw(Term& term) override {
		if (!_visible)
			return;

		term.moveCursor(_position.x, _position.y);
		term.setColor(fgColor, bgColor);

		// Truncate text if it's too long
		std::string output = display_text;
		if (static_cast<int>(output.length()) > _size.width) {
			output = output.substr(0, _size.width - 3) + "...";
		}

		std::cout << output;
		term.resetColor();
	}

	void setText(const std::string& new_text) {
		display_text = new_text;
		text         = new_text;
		_size.width  = std::max(1, static_cast<int>(new_text.length()));
	}
};
