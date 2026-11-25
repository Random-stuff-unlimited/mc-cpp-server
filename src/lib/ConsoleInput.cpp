#include "ConsoleInput.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <termios.h>
#include <unistd.h>

namespace ConsoleInput {
	namespace {
		constexpr const char* PROMPT	  = "> ";
		constexpr size_t	  MAX_HISTORY = 200;

		std::mutex	   g_mutex; // The terminal: the editor's redraws and the log lines
		bool		   g_active = false;
		std::string	   g_buffer;
		size_t		   g_cursor = 0; // A byte index into g_buffer, on a character boundary
		struct termios g_original;

		bool continuation(char c) { return (static_cast<unsigned char>(c) & 0xC0) == 0x80; }
		size_t characters(const std::string& text, size_t from, size_t to) {
			size_t count = 0;
			for (size_t i = from; i < to; i++) count += !continuation(text[i]);
			return count;
		}

		// The prompt and the line, the cursor where it is. g_mutex held
		void redraw() {
			std::string out = "\r\033[K";
			out += PROMPT;
			out += g_buffer;
			size_t after = characters(g_buffer, g_cursor, g_buffer.size());
			if (after > 0) out += "\033[" + std::to_string(after) + "D";
			std::fwrite(out.data(), 1, out.size(), stdout);
			std::fflush(stdout);
		}

		void restoreTerminal() {
			std::lock_guard lock(g_mutex);
			if (!g_active) return;
			g_active = false;
			tcsetattr(STDIN_FILENO, TCSANOW, &g_original);
			std::fputs("\r\033[K", stdout);
			std::fflush(stdout);
		}

		// No echo, a key at a time; Ctrl+C still raises SIGINT
		bool enterRawMode() {
			if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO) || tcgetattr(STDIN_FILENO, &g_original) != 0) return false;
			struct termios raw = g_original;
			raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO | IEXTEN));
			raw.c_iflag &= static_cast<tcflag_t>(~(IXON | ICRNL));
			raw.c_cc[VMIN]	= 1;
			raw.c_cc[VTIME] = 0;
			if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) return false;
			std::atexit(restoreTerminal);
			return true;
		}

		bool readByte(char& c) { return ::read(STDIN_FILENO, &c, 1) == 1; }

		void setLine(const std::string& line) {
			g_buffer = line;
			g_cursor = g_buffer.size();
		}
	} // namespace

	void print(const std::string& text) {
		std::lock_guard lock(g_mutex);
		if (!g_active) {
			std::cout << text << std::endl;
			return;
		}
		std::string out = "\r\033[K" + text + "\r\n";
		std::fwrite(out.data(), 1, out.size(), stdout);
		redraw();
	}

	Completion complete(const std::string& line, const std::vector<std::string>& candidates) {
		size_t		start  = line.find_last_of(' ');
		start			   = start == std::string::npos ? 0 : start + 1;
		std::string prefix = line.substr(start);
		auto		lower  = [](std::string text) {
			   std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return std::tolower(c); });
			   return text;
		};
		Completion result{line, {}};
		std::string wanted = lower(prefix);
		for (const std::string& candidate : candidates) {
			if (lower(candidate).rfind(wanted, 0) == 0 && std::find(result.matches.begin(), result.matches.end(), candidate) == result.matches.end()) {
				result.matches.push_back(candidate);
			}
		}
		std::sort(result.matches.begin(), result.matches.end());
		if (result.matches.empty()) return result;
		// The longest prefix they share (ignoring case: the first match's spelling)
		std::string common = result.matches[0];
		for (const std::string& match : result.matches) {
			size_t n = 0;
			while (n < common.size() && n < match.size() && std::tolower(static_cast<unsigned char>(common[n])) == std::tolower(static_cast<unsigned char>(match[n]))) n++;
			common.resize(n);
		}
		if (common.size() < prefix.size()) return result;
		result.line = line.substr(0, start) + common + (result.matches.size() == 1 ? " " : "");
		return result;
	}

	void run(const std::function<void(const std::string&)>& onLine, const Completer& completer) {
		if (!enterRawMode()) {
			std::string line;
			while (std::getline(std::cin, line)) onLine(line);
			return;
		}
		std::vector<std::string> history;
		size_t					 position = 0; // In history; history.size(): the line being typed
		std::string				 draft;		   // That line, while browsing the history
		{
			std::lock_guard lock(g_mutex);
			g_active = true;
			redraw();
		}
		char c;
		while (readByte(c)) {
			std::unique_lock lock(g_mutex);
			if (c == '\r' || c == '\n') {
				std::string line = g_buffer;
				g_buffer.clear();
				g_cursor = 0;
				std::string echo = "\r\033[K" + std::string(PROMPT) + line + "\r\n";
				std::fwrite(echo.data(), 1, echo.size(), stdout);
				redraw();
				if (line.find_first_not_of(' ') != std::string::npos && (history.empty() || history.back() != line)) {
					history.push_back(line);
					if (history.size() > MAX_HISTORY) history.erase(history.begin());
				}
				position = history.size();
				lock.unlock();
				onLine(line);
				continue;
			}
			if (c == 4) { // Ctrl+D: the end of the input, on an empty line
				if (g_buffer.empty()) break;
				continue;
			}
			if (c == 127 || c == 8) { // Backspace
				if (g_cursor == 0) continue;
				size_t from = g_cursor - 1;
				while (from > 0 && continuation(g_buffer[from])) from--;
				g_buffer.erase(from, g_cursor - from);
				g_cursor = from;
			} else if (c == 1) { // Ctrl+A
				g_cursor = 0;
			} else if (c == 5) { // Ctrl+E
				g_cursor = g_buffer.size();
			} else if (c == 21) { // Ctrl+U
				g_buffer.erase(0, g_cursor);
				g_cursor = 0;
			} else if (c == 23) { // Ctrl+W: the word before the cursor
				size_t from = g_cursor;
				while (from > 0 && g_buffer[from - 1] == ' ') from--;
				while (from > 0 && g_buffer[from - 1] != ' ') from--;
				g_buffer.erase(from, g_cursor - from);
				g_cursor = from;
			} else if (c == '\t') {
				if (!completer) continue;
				std::string before = g_buffer.substr(0, g_cursor);
				lock.unlock();
				Completion done = complete(before, completer(before));
				lock.lock();
				if (g_buffer.substr(0, g_cursor) != before) continue; // Changed meanwhile
				if (done.matches.size() > 1 && done.line == before) {
					std::string list;
					for (const std::string& match : done.matches) list += match + "  ";
					std::string out = "\r\033[K" + list + "\r\n";
					std::fwrite(out.data(), 1, out.size(), stdout);
				}
				g_buffer.replace(0, g_cursor, done.line);
				g_cursor = done.line.size();
			} else if (c == 27) { // Escape sequences: the arrows, Home, End, Delete
				char a, b;
				if (!readByte(a) || (a != '[' && a != 'O') || !readByte(b)) continue;
				if (b == 'A' || b == 'B') {
					if (history.empty()) continue;
					if (position == history.size()) draft = g_buffer;
					if (b == 'A' && position > 0) position--;
					else if (b == 'B' && position < history.size()) position++;
					setLine(position == history.size() ? draft : history[position]);
				} else if (b == 'C') {
					if (g_cursor < g_buffer.size()) {
						g_cursor++;
						while (g_cursor < g_buffer.size() && continuation(g_buffer[g_cursor])) g_cursor++;
					}
				} else if (b == 'D') {
					if (g_cursor > 0) {
						g_cursor--;
						while (g_cursor > 0 && continuation(g_buffer[g_cursor])) g_cursor--;
					}
				} else if (b == 'H') {
					g_cursor = 0;
				} else if (b == 'F') {
					g_cursor = g_buffer.size();
				} else if (b >= '0' && b <= '9') {
					char end;
					if (!readByte(end) || end != '~') continue;
					if (b == '3' && g_cursor < g_buffer.size()) { // Delete
						size_t to = g_cursor + 1;
						while (to < g_buffer.size() && continuation(g_buffer[to])) to++;
						g_buffer.erase(g_cursor, to - g_cursor);
					} else if (b == '1' || b == '7') {
						g_cursor = 0;
					} else if (b == '4' || b == '8') {
						g_cursor = g_buffer.size();
					}
				}
			} else if (static_cast<unsigned char>(c) >= 0x20) {
				g_buffer.insert(g_cursor, 1, c);
				g_cursor++;
			}
			redraw();
		}
		restoreTerminal();
	}
} // namespace ConsoleInput
