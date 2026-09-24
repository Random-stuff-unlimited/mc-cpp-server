#ifndef COMPRESSION_HPP
#define COMPRESSION_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

namespace compression {
	// zlib stream (network packets, region files)
	std::vector<uint8_t> zlibCompress(const uint8_t* data, size_t size, int level);
	// Throws unless the output is exactly expectedSize bytes
	std::vector<uint8_t> zlibDecompress(const uint8_t* data, size_t size, size_t expectedSize);

	// Unknown output size (vanilla files): zlib or gzip stream, throws past maxSize
	std::vector<uint8_t> inflateUnknownSize(const uint8_t* data, size_t size, bool gzip, size_t maxSize);
} // namespace compression

#endif
