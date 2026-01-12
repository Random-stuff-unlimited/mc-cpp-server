#include "network/buffer.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>


// Byte Array - Variable size
// Encodes: A sequence of raw bytes
// Notes: Used for raw data, chunk data, compressed data, etc.
//        Length must be known from context

namespace DataType {
    namespace ByteArray {
        // Read a fixed number of bytes from buffer
        std::vector<uint8_t> readBytes(Buffer& buffer, size_t length) {
            std::vector<uint8_t> data;
            data.reserve(length);

            for (size_t i = 0; i < length; ++i) {
                data.push_back(buffer.readByte());
            }

            return data;
        }

        // Write bytes to buffer
        void writeBytes(Buffer& buffer, const std::vector<uint8_t>& data) {
            buffer.writeBytes(data);
        }

        // Write bytes to buffer from raw pointer
        void writeBytes(Buffer& buffer, const uint8_t* data, size_t length) {
            for (size_t i = 0; i < length; ++i) {
                buffer.writeByte(data[i]);
            }
        }
    }
}
