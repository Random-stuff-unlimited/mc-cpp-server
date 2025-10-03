#pragma once

#include "event.hpp"
#include "terminal.hpp"

#include <string>

class Base {
  protected:
	Position _position;
	Size _size;
	bool _visible   = true;
	bool _focusable = false;
	bool _hasFocus  = false;
	std::string text;
	Color fgColor   = Color::WHITE;
	BGColor bgColor = BGColor::RESET;

  public:
	Base(Position pos = {0, 0}, Size sz = {1, 1}) : _position(pos), _size(sz) {}
	virtual ~Base() = default;

	Position getPosition() const {
		return _position;
	}
	Size getSize() const {
		return _size;
	}
	bool isVisible() const {
		return _visible;
	}
	bool isFocusable() const {
		return _focusable;
	}
	bool isFocused() const {
		return _hasFocus;
	}

	void setPosition(Position pos) {
		_position = pos;
	}
	void setSize(Size sz) {
		_size = sz;
	}
	void setVisible(bool v) {
		_visible = v;
	}
	void setFocus(bool f) {
		_hasFocus = f;
	}
	void setText(const std::string& t) {
		text = t;
	}
	void setColors(Color fg, BGColor bg = BGColor::RESET) {
		fgColor = fg;
		bgColor = bg;
	}

	virtual void draw(Term& term) = 0;
	virtual bool handleEvent(const Event& event) {
		return false;
	}
	virtual void onFocusGained() {}
	virtual void onFocusLost() {}
};
