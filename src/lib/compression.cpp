#include "lib/compression.hpp"

#include <algorithm>
#include <array>
#include <libdeflate.h>
#include <stdexcept>
#include <zlib.h>

namespace compression {

	namespace {
		// libdeflate objects aren't thread-safe but are costly to create: one per thread, created on first use
		struct ThreadCompressors {
			std::array<libdeflate_compressor*, 13> compressors{};
			libdeflate_decompressor*			   decompressor = nullptr;

			~ThreadCompressors() {
				for (libdeflate_compressor* compressor : compressors) {
					if (compressor) libdeflate_free_compressor(compressor);
				}
				if (decompressor) libdeflate_free_decompressor(decompressor);
			}

			libdeflate_compressor* compressor(int level) {
				level = std::clamp(level, 0, 12);
				if (!compressors[level]) compressors[level] = libdeflate_alloc_compressor(level);
				if (!compressors[level]) throw std::runtime_error("libdeflate_alloc_compressor failed");
				return compressors[level];
			}

			libdeflate_decompressor* decompressorInstance() {
				if (!decompressor) decompressor = libdeflate_alloc_decompressor();
				if (!decompressor) throw std::runtime_error("libdeflate_alloc_decompressor failed");
				return decompressor;
			}
		};
		thread_local ThreadCompressors t_compressors;
	} // namespace

	void zlibCompressAppend(const uint8_t* data, size_t size, int level, std::vector<uint8_t>& out) {
		libdeflate_compressor* compressor = t_compressors.compressor(level);
		size_t				   start	  = out.size();
		out.resize(start + libdeflate_zlib_compress_bound(compressor, size));
		size_t written = libdeflate_zlib_compress(compressor, data, size, out.data() + start, out.size() - start);
		if (written == 0) throw std::runtime_error("zlib compression failed");
		out.resize(start + written);
	}

	std::vector<uint8_t> zlibCompress(const uint8_t* data, size_t size, int level) {
		std::vector<uint8_t> out;
		zlibCompressAppend(data, size, level, out);
		return out;
	}

	bool zlibDecompressInto(const uint8_t* data, size_t size, uint8_t* out, size_t expectedSize) {
		size_t actual = 0;
		return libdeflate_zlib_decompress(t_compressors.decompressorInstance(), data, size, out, expectedSize, &actual) == LIBDEFLATE_SUCCESS &&
			   actual == expectedSize;
	}

	std::vector<uint8_t> zlibDecompress(const uint8_t* data, size_t size, size_t expectedSize) {
		std::vector<uint8_t> out(expectedSize);
		if (!zlibDecompressInto(data, size, out.data(), expectedSize)) throw std::runtime_error("zlib decompression failed");
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
