#pragma once
#include <asm-generic/ioctls.h>
#include <iostream>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

enum class Color {
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

enum class BGColor {
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

struct Position {
	int x, y;
	Position(int x = 0, int y = 0) : x(x), y(y) {}
	Position operator+(const Position& other) const {
		return {x + other.x, y + other.y};
	}
};

struct Size {
	int width, height;
	Size(int w = 0, int h = 0) : width(w), height(h) {}
};

class Term {
  private:
	struct termios original_termios;
	bool raw_mode_enabled = false;

  public:
	Term();
	~Term() {
		disableRawMode();
	}
	void enableRawMode() {
		if (raw_mode_enabled)
			return;

		tcgetattr(STDIN_FILENO, &original_termios);
		struct termios raw = original_termios;
		raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
		raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
		raw.c_cflag |= (CS8);
		raw.c_oflag &= ~(OPOST);
		raw.c_cc[VMIN]  = 0;
		raw.c_cc[VTIME] = 1;

		tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
		raw_mode_enabled = true;
	}

	void disableRawMode() {
		if (!raw_mode_enabled)
			return;
		tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
		raw_mode_enabled = false;
	}

	void clearScreen() const {
		std::cout << "\033[2J";
	}
	void clear_screen() const {
		std::cout << "\033[2J";
	}
	void hide_cursor() const {
		std::cout << "\033[?25l";
	}
	void show_cursor() const {
		std::cout << "\033[?25h";
	}
	void move_cursor(int x, int y) const {
		std::cout << "\033[" << y + 1 << ";" << x + 1 << "H";
	}
	void flush() const {
		std::cout.flush();
	}
	void set_color(Color fg, BGColor bg = BGColor::RESET) const {
		if (fg != Color::RESET)
			std::cout << "\033[" << static_cast<int>(fg) << "m";
		if (bg != BGColor::RESET)
			std::cout << "\033[" << static_cast<int>(bg) << "m";
	}
	void reset_color() const {
		std::cout << "\033[0m";
	}
	char read_key() const {
		char c;
		return (read(STDIN_FILENO, &c, 1) == 1) ? c : 0;
	}
	Size get_terminal_size() const {
		struct winsize w;
		ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
		return {w.ws_col, w.ws_row};
	}
};
