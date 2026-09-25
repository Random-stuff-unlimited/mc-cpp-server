#ifndef TEST_HPP
#define TEST_HPP

#include <functional>
#include <iostream>
#include <string>
#include <vector>

// Minimal test runner: TEST(name) { CHECK(condition); CHECK_EQ(a, b); }, run by make test
namespace test {
	struct Case {
		const char*			  name;
		std::function<void()> run;
	};
	inline std::vector<Case>& cases() {
		static std::vector<Case> all;
		return all;
	}
	inline int& failures() {
		static int count = 0;
		return count;
	}
	struct Register {
		Register(const char* name, std::function<void()> run) { cases().push_back({name, std::move(run)}); }
	};
	inline void fail(const char* file, int line, const std::string& message) {
		if (failures()++ < 20) std::cerr << "  FAIL " << file << ":" << line << ": " << message << "\n";
	}
} // namespace test

#define TEST_CONCAT2(a, b) a##b
#define TEST_CONCAT(a, b) TEST_CONCAT2(a, b)
#define TEST(name)                                                                     \
	static void		  name();                                                          \
	static test::Register TEST_CONCAT(register_, name)(#name, name);                 \
	static void		  name()
#define CHECK(condition)                                                               \
	do {                                                                               \
		if (!(condition)) test::fail(__FILE__, __LINE__, #condition);                  \
	} while (0)
#define CHECK_EQ(a, b)                                                                 \
	do {                                                                               \
		auto _a = (a);                                                                 \
		auto _b = (b);                                                                 \
		if (!(_a == _b)) test::fail(__FILE__, __LINE__, #a " == " #b " (" + std::to_string(_a) + " vs " + std::to_string(_b) + ")"); \
	} while (0)

#endif
