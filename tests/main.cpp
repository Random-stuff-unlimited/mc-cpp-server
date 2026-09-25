#include "Test.hpp"
#include "logger.hpp"

int main() {
	initializeGlobalLogger();
	for (const test::Case& c : test::cases()) {
		int before = test::failures();
		c.run();
		std::cout << (test::failures() == before ? "  ok   " : "  FAIL ") << c.name << "\n";
	}
	std::cout << (test::failures() == 0 ? "All tests passed\n" : std::to_string(test::failures()) + " check(s) failed\n");
	return test::failures() == 0 ? 0 : 1;
}
