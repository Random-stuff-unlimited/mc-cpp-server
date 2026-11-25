#ifndef CONSOLE_INPUT_HPP
#define CONSOLE_INPUT_HPP

#include <functional>
#include <string>
#include <vector>

// The server console's input (vanilla's dedicated server uses JLine for it): on a terminal, a line editor with a
// "> " prompt, the arrows, history (up/down) and Tab completion; the log lines print above the line being typed.
// Not a terminal (a pipe, a file): plain lines
namespace ConsoleInput {
	// What Tab completes: the words that may follow `line` (the whole line as typed so far)
	using Completer = std::function<std::vector<std::string>(const std::string& line)>;

	// Reads lines until the input ends, giving each one (without its end) to onLine. Blocks: its own thread
	void run(const std::function<void(const std::string&)>& onLine, const Completer& complete);
	// A line of output (the logger's), above the line being typed when the editor is active
	void print(const std::string& text);

	// Tab: the matches of the line's last word among the candidates, and the line with that word extended as far
	// as all of them agree (with a space after a single match)
	struct Completion {
		std::string				 line;
		std::vector<std::string> matches;
	};
	Completion complete(const std::string& line, const std::vector<std::string>& candidates);
} // namespace ConsoleInput

#endif
