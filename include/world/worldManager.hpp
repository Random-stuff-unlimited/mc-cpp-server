#ifndef WORLD_MANAGER_HPP
#define WORLD_MANAGER_HPP

#include <cstdint>
#include <vector>
#include <zlib.h>

class WorldManager {
  private:
  public:
	std::vector<uint8_t> decompressGzip(const std::vector<uint8_t>& compressed);
};

#endif
