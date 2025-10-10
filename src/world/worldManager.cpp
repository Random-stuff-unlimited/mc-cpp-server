#include "world/worldManager.hpp"

#include <climits>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include <zconf.h>
#include <zlib.h>

std::vector<uint8_t> WorldManager::decompressGzip(std::filesystem::path compressedFilePath) {
	// Read file into memory
	std::ifstream file(compressedFilePath, std::ios::binary);
	if (!file) {
		throw std::runtime_error("Could not open file: " + compressedFilePath.string());
	}

	std::vector<uint8_t> compressed((std::istreambuf_iterator<char>(file)),
									std::istreambuf_iterator<char>());
	file.close();

	// Initialize zlib stream
	z_stream stream;
	std::memset(&stream, 0, sizeof(stream));

	// 16 + MAX_WBITS tells zlib to decode gzip format
	if (inflateInit2(&stream, 16 + MAX_WBITS) != Z_OK) {
		throw std::runtime_error("Failed to initialize gzip decompression");
	}

	// Allocate output buffer (level.dat is usually < 10MB)
	std::vector<uint8_t> decompressed(10 * 1024 * 1024);

	stream.avail_in	 = compressed.size();
	stream.next_in	 = compressed.data();
	stream.avail_out = decompressed.size();
	stream.next_out	 = decompressed.data();

	int ret = inflate(&stream, Z_FINISH);

	if (ret != Z_STREAM_END) {
		inflateEnd(&stream);
		throw std::runtime_error("Decompression failed: " + std::to_string(ret));
	}

	// Resize to actual decompressed size
	decompressed.resize(stream.total_out);
	inflateEnd(&stream);

	return decompressed;
}
