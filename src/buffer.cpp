#include "buffer.hpp"

Buffer::Buffer() : pos(0) {}
Buffer::Buffer(const std::vector<uint8_t> &data) : data(data), pos(0) {}

int Buffer::readVarInt() {
	int value = 0, position = 0;
	uint8_t currentByte;

	do {
		if (pos >= data.size()) throw std::runtime_error("Buffer underflow");
		currentByte = data[pos++];
		value |= (currentByte & 0x7F) << position;
		position += 7;
		if (position >= 32) throw std::runtime_error("VarInt too big");
	} while (currentByte & 0x80);

	return value;
}

void Buffer::writeVarInt(int value) {
	while (true) {
		if ((value & ~0x7F) == 0) {
			data.push_back(static_cast<uint8_t>(value));
			return;
		} else {
			data.push_back(static_cast<uint8_t>((value & 0x7F) | 0x80));
			value >>= 7;
		}
	}
}

std::string Buffer::readString() {
	int len = readVarInt();
	if (pos + len > data.size()) throw std::runtime_error("Buffer underflow on string");
	std::string result(reinterpret_cast<char*>(&data[pos]), len);
	pos += len;
	return result;
}

void Buffer::writeString(const std::string &str) {
	writeVarInt(static_cast<int>(str.size()));
	data.insert(data.end(), str.begin(), str.end());
}

const std::vector<uint8_t>& Buffer::getData() const {
	return data;
}

size_t Buffer::remaining() const {
	return data.size() - pos;
}

uint16_t Buffer::readUShort() {
    if (pos + 2 > data.size()) throw std::runtime_error("Buffer underflow");
    uint16_t val = (data[pos] << 8) | data[pos+1]; // big-endian
    pos += 2;
    return val;
}

uint64_t Buffer::readUInt64() {
    if (pos + 8 > data.size()) throw std::runtime_error("Buffer overflow");
    uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value = (value << 8) | data[pos++];
    }
    return value;
}
