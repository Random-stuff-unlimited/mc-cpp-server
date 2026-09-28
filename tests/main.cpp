#include "Test.hpp"
#include "logger.hpp"

#include <cstdlib>
#include <iostream>

int main() {
	initializeGlobalLogger();
	for (const test::Case& c : test::cases()) {
		int before = test::failures();
		c.run();
		std::cout << (test::failures() == before ? "  ok   " : "  FAIL ") << c.name << "\n";
	}
	std::cout << (test::failures() == 0 ? "All tests passed\n" : std::to_string(test::failures()) + " check(s) failed\n");
	// Without the destructors of statics and thread_locals: test players left in the network flush list (they have no
	// socket, nothing ever flushes them) would be destroyed after the id manager they give their id back to
	std::cout.flush();
	std::_Exit(test::failures() == 0 ? 0 : 1);
}
