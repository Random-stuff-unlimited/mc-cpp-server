#ifndef MD5_HPP
#define MD5_HPP

#include <cstdint>
#include <string>
#include <vector>

class MD5 {
  public:
	static std::vector<uint8_t> hash(const std::string& input);
};

#endif
