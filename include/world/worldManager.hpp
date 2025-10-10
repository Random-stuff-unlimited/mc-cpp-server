#ifndef WORLD_MANAGER_HPP
#define WORLD_MANAGER_HPP

#include <cstdint>
#include <filesystem>
#include <vector>
#include <zlib.h>

class WorldManager {
  private:
  public:
	std::vector<uint8_t> decompressGzip(std::filesystem::path compressedFilePath);
};

#endif
