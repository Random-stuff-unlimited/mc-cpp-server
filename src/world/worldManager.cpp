#include "world/worldManager.hpp"

#include <zlib.h>
#include <stdexcept>
#include <vector>
#include <cstdint>
#include <zconf.h>

std::vector<uint8_t> WorldManager::decompressGzip(const std::vector<uint8_t>& compressed) {
    z_stream stream{};
    stream.avail_in = compressed.size();
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<uint8_t*>(compressed.data()));
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;

    int ret = inflateInit2(&stream, 15 + 32);
    if (ret != Z_OK) {
        throw std::runtime_error("Failed to initialize zlib");
    }

    std::vector<uint8_t> decompressed;
    decompressed.resize(compressed.size() * 2);

    do {
        stream.avail_out = decompressed.size();
        stream.next_out = reinterpret_cast<Bytef*>(decompressed.data());

        ret = inflate(&stream, Z_NO_FLUSH);
        if (ret == Z_NEED_DICT || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
            inflateEnd(&stream);
            throw std::runtime_error("Failed to decompress data");
        }

        decompressed.resize(stream.total_out);
    } while (ret == Z_OK);

    inflateEnd(&stream);
    return decompressed;
}
