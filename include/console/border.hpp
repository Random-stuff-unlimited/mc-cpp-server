#pragma once
#include "console/base.hpp"
#include "console/terminal.hpp"

#include <iostream>
#include <string>

class Border : public Base {
  private:
	std::string title;

  public:
	Border(Size sz, Position pos = {0, 0}, const std::string& border_title = "")
	    : Base(pos, sz), title(border_title) {}

	void draw(Term& term) override {
		if (!_visible)
			return;

		term.setColor(fgColor, bgColor);

		// Top border
		term.moveCursor(_position.x, _position.y);
		std::cout << "┌";
		if (!title.empty() && static_cast<int>(title.length()) < _size.width - 4) {
			std::cout << "─ " << title << " ";
			int remaining = _size.width - 4 - static_cast<int>(title.length()) - 2;
			for (int i = 0; i < remaining; ++i)
				std::cout << "─";
		} else {
			for (int i = 0; i < _size.width - 2; ++i)
				std::cout << "─";
		}
		std::cout << "┐";

		// Side borders
		for (int y = 1; y < _size.height - 1; ++y) {
			term.moveCursor(_position.x, _position.y + y);
			std::cout << "│";
			term.moveCursor(_position.x + _size.width - 1, _position.y + y);
			std::cout << "│";
		}

		// Bottom border
		term.moveCursor(_position.x, _position.y + _size.height - 1);
		std::cout << "└";
		for (int i = 0; i < _size.width - 2; ++i)
			std::cout << "─";
		std::cout << "┘";

		term.resetColor();
	}
};
