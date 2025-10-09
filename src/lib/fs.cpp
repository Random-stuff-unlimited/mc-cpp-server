#include <filesystem>
#include <lib/filesystem.hpp>
#include <limits.h>
#include <linux/limits.h>
#include <sys/types.h>
#include <unistd.h>

std::filesystem::path getPath() {
	char buffer[PATH_MAX];
	ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
	if (len != -1) {
		buffer[len] = '\0';
		return std::filesystem::path(buffer);
	}
	return {};
}
