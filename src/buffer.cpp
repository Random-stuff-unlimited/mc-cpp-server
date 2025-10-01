#include "buffer.hpp"

Buffer::Buffer() : _pos(0) {}
Buffer::Buffer(const std::vector<uint8_t> &data) : _data(data), _pos(0) {}

uint8_t Buffer::readByte() {
    if (_pos >= _data.size()) throw std::runtime_error("Buffer underflow on byte");
    return _data[_pos++];
}

void Buffer::writeByte(uint8_t byte) {
    _data.push_back(byte);
}

int Buffer::readVarInt() {
    int value = 0, position = 0;
    uint8_t currentByte;

    do {
        currentByte = readByte();
        value |= (currentByte & 0x7F) << position;
        position += 7;
        if (position >= 32) throw std::runtime_error("VarInt too big");
    } while (currentByte & 0x80);

    return value;
}

void Buffer::writeVarInt(int value) {
    while (true) {
        if ((value & ~0x7F) == 0) {
            writeByte(static_cast<uint8_t>(value)); // <- utilise writeByte
            return;
        } else {
            writeByte(static_cast<uint8_t>((value & 0x7F) | 0x80));
            value >>= 7;
        }
    }
}

std::string Buffer::readString() {
	int len = readVarInt();
	if (_pos + len > _data.size()) throw std::runtime_error("Buffer underflow on string");
	std::string result(reinterpret_cast<char*>(&_data[_pos]), len);
	_pos += len;
	return result;
}

void Buffer::writeString(const std::string &str) {
	writeVarInt(static_cast<int>(str.size()));
	_data.insert(_data.end(), str.begin(), str.end());
}

const std::vector<uint8_t>& Buffer::getData() const {
	return _data;
}

size_t Buffer::remaining() const {
	return _data.size() - _pos;
}

uint16_t Buffer::readUShort() {
    uint16_t val = (readByte() << 8) | readByte();
    return val;
}

uint64_t Buffer::readUInt64() {
    uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value = (value << 8) | readByte();
    }
    return value;
}
