#include "lib/compression.hpp"

#include <algorithm>
#include <stdexcept>
#include <zlib.h>

namespace compression {

	std::vector<uint8_t> zlibCompress(const uint8_t* data, size_t size, int level) {
		uLongf				 outSize = compressBound(size);
		std::vector<uint8_t> out(outSize);
		if (compress2(out.data(), &outSize, data, size, level) != Z_OK) throw std::runtime_error("zlib compression failed");
		out.resize(outSize);
		return out;
	}

	std::vector<uint8_t> zlibDecompress(const uint8_t* data, size_t size, size_t expectedSize) {
		std::vector<uint8_t> out(expectedSize);
		uLongf				 outSize = expectedSize;
		if (uncompress(out.data(), &outSize, data, size) != Z_OK || outSize != expectedSize) {
			throw std::runtime_error("zlib decompression failed");
		}
		return out;
	}

	std::vector<uint8_t> inflateUnknownSize(const uint8_t* data, size_t size, bool gzip, size_t maxSize) {
		z_stream stream{};
		if (inflateInit2(&stream, gzip ? 16 + MAX_WBITS : MAX_WBITS) != Z_OK) throw std::runtime_error("inflateInit failed");

		std::vector<uint8_t> out(size * 4 + 1024);
		stream.next_in	= const_cast<Bytef*>(data);
		stream.avail_in = static_cast<uInt>(size);

		int result = Z_OK;
		while (result != Z_STREAM_END) {
			if (stream.total_out == out.size()) {
				if (out.size() >= maxSize) {
					inflateEnd(&stream);
					throw std::runtime_error("Decompressed data too large");
				}
				out.resize(std::min(out.size() * 2, maxSize));
			}
			stream.next_out	 = out.data() + stream.total_out;
			stream.avail_out = static_cast<uInt>(out.size() - stream.total_out);
			result			 = inflate(&stream, Z_NO_FLUSH);
			if (result != Z_OK && result != Z_STREAM_END) {
				inflateEnd(&stream);
				throw std::runtime_error("Corrupted compressed data");
			}
		}
		out.resize(stream.total_out);
		inflateEnd(&stream);
		return out;
	}

} // namespace compression
