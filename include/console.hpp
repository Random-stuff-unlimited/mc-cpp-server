#pragma once

#include <termios.h>

enum class Color
{
	RESET          = 0,
	BLACK          = 30,
	RED            = 31,
	GREEN          = 32,
	YELLOW         = 33,
	BLUE           = 34,
	MAGENTA        = 35,
	CYAN           = 36,
	WHITE          = 37,
	BRIGHT_BLACK   = 90,
	BRIGHT_RED     = 91,
	BRIGHT_GREEN   = 92,
	BRIGHT_YELLOW  = 93,
	BRIGHT_BLUE    = 94,
	BRIGHT_MAGENTA = 95,
	BRIGHT_CYAN    = 96,
	BRIGHT_WHITE   = 97
};

enum class BGColor
{
	RESET          = 0,
	BLACK          = 40,
	RED            = 41,
	GREEN          = 42,
	YELLOW         = 43,
	BLUE           = 44,
	MAGENTA        = 45,
	CYAN           = 46,
	WHITE          = 47,
	BRIGHT_BLACK   = 100,
	BRIGHT_RED     = 101,
	BRIGHT_GREEN   = 102,
	BRIGHT_YELLOW  = 103,
	BRIGHT_BLUE    = 104,
	BRIGHT_MAGENTA = 105,
	BRIGHT_CYAN    = 106,
	BRIGHT_WHITE   = 107
};

struct Position
{
	int x, y;
	Position(int x = 0, int y = 0) : x(x), y(y) {}
	Position operator+(const Position& other) const
	{
		return {x + other.x, y + other.y};
	}
};

struct Size
{
	int width, height;
	Size(int w = 0, int h = 0) : width(w), height(h) {}
};

class Console
{
  private:
	struct termios original_termios;
	bool raw_mode_enabled = false;

  public:
	Console();
	~Console()
	{
		disableRawMode();
	}
};
