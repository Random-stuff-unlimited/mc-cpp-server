#include "network/buffer.hpp"
#include "lib/UUID.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

Buffer::Buffer() : _pos(0) {}
Buffer::Buffer(const std::vector<uint8_t>& data) : _data(data), _pos(0) {}


// ============================================================
// ==========           MÉTHODES DE LECTURE          ==========
// ============================================================

uint8_t Buffer::readByte() {
	if (_pos >= _data.size()) throw std::runtime_error("Buffer underflow on byte");
	return _data[_pos++];
}

bool Buffer::readBool() {
	return readByte() != 0;
}

int32_t Buffer::readInt() {
	int32_t value = 0;
	for (int i = 0; i < 4; ++i)
		value = (value << 8) | readByte();
	return value;
}

uint16_t Buffer::readUShort() {
	return (readByte() << 8) | readByte();
}

uint64_t Buffer::readUInt64() {
	uint64_t value = 0;
	for (int i = 0; i < 8; ++i)
		value = (value << 8) | readByte();
	return value;
}

int32_t Buffer::readVarInt() {
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

int64_t Buffer::readVarInt64() {
	int64_t value = 0;
	int position = 0;
	uint8_t currentByte;
	do {
		currentByte = readByte();
		value |= ((int64_t)(currentByte & 0x7F)) << position;
		position += 7;
		if (position >= 64) throw std::runtime_error("VarLong too big");
	} while (currentByte & 0x80);
	return value;
}

std::string Buffer::readString(int maxLength) {
	int len = readVarInt();
	if (maxLength > 0 && len > maxLength)
		throw std::runtime_error("String length exceeds maximum allowed");
	if (_pos + len > _data.size())
		throw std::runtime_error("Buffer underflow on string");

	std::string result(reinterpret_cast<char*>(&_data[_pos]), len);
	_pos += len;
	return result;
}

std::string Buffer::readString() {
	return readString(0);
}

std::vector<std::string> Buffer::readStringArray() {
	int count = readVarInt();
	if (count < 0) throw std::runtime_error("Negative array length");

	std::vector<std::string> result;
	result.reserve(count);
	for (int i = 0; i < count; ++i)
		result.push_back(readString());
	return result;
}

std::vector<int> Buffer::readVarIntArray() {
	int count = readVarInt();
	if (count < 0) throw std::runtime_error("Negative array length");

	std::vector<int> result;
	result.reserve(count);
	for (int i = 0; i < count; ++i)
		result.push_back(readVarInt());
	return result;
}

int64_t Buffer::readInt64() {
    int64_t value = 0;
    for (int i = 0; i < 8; ++i)
        value = (value << 8) | readByte();
    return value;
}


// ============================================================
// ==========           MÉTHODES D'ÉCRITURE          ==========
// ============================================================

void Buffer::writeByte(uint8_t byte) {
	_data.push_back(byte);
}

void Buffer::prependByte(uint8_t byte) {
    _data.insert(_data.begin(), byte);
}

void Buffer::writeBytes(const std::string& data) {
	_data.insert(_data.end(), data.begin(), data.end());
}

void Buffer::writeBytes(const std::vector<uint8_t>& data) {
	_data.insert(_data.end(), data.begin(), data.end());
}

void Buffer::prependBytes(const std::string& data) {
    _data.insert(_data.begin(), data.begin(), data.end());
}

void Buffer::writeBool(bool value) {
	writeByte(value ? 1 : 0);
}

void Buffer::writeInt(int32_t value) {
	for (int i = 3; i >= 0; --i)
		writeByte((value >> (i * 8)) & 0xFF);
}

void Buffer::writeUInt(uint32_t value) {
	for (int i = 3; i >= 0; --i)
		writeByte((value >> (i * 8)) & 0xFF);
}

void Buffer::writeUShort(uint16_t value) {
	writeByte((value >> 8) & 0xFF);
	writeByte(value & 0xFF);
}

void Buffer::writeInt64(int64_t value) {
	for (int i = 7; i >= 0; --i)
		writeByte((value >> (i * 8)) & 0xFF);
}

void Buffer::writeFloat(float value) {
	union { float f; uint32_t i; } u;
	u.f = value;
	writeUInt(u.i);
}

void Buffer::writeDouble(double value) {
	union { double d; uint64_t i; } u;
	u.d = value;
	writeInt64(u.i);
}

void Buffer::writeVarInt(int value) {
	while (true) {
		if ((value & ~0x7F) == 0) {
			writeByte(value);
			return;
		}
		writeByte((value & 0x7F) | 0x80);
		value >>= 7;
	}
}

void Buffer::prependVarInt(int value) {
    std::vector<uint8_t> tmp;

    while (true) {
        if ((value & ~0x7F) == 0) {
            tmp.push_back(static_cast<uint8_t>(value));
            break;
        }
        tmp.push_back(static_cast<uint8_t>((value & 0x7F) | 0x80));
        value >>= 7;
    }
    _data.insert(_data.begin(), tmp.begin(), tmp.end());
}

void Buffer::writeVarInt64(int64_t value) {
	while (true) {
		if ((value & ~0x7FL) == 0) {
			writeByte((uint8_t)value);
			return;
		}
		writeByte((uint8_t)((value & 0x7F) | 0x80));
		value >>= 7;
	}
}

void Buffer::writeString(const std::string& str) {
	writeVarInt((int)str.size());
	writeBytes(str);
}

void Buffer::writeUUID(const UUID& uuid) {
	uint64_t msb = uuid.getMostSigBits();
	uint64_t lsb = uuid.getLeastSigBits();
	for (int i = 7; i >= 0; --i)
		writeByte((msb >> (i * 8)) & 0xFF);
	for (int i = 7; i >= 0; --i)
		writeByte((lsb >> (i * 8)) & 0xFF);
}

void Buffer::writePosition(int32_t x, int32_t y, int32_t z) {
	int64_t packed = ((int64_t)(x & 0x3FFFFFF) << 38) |
					 ((int64_t)(y & 0xFFF) << 26) |
					 ((int64_t)(z & 0x3FFFFFF));
	writeInt64(packed);
}

void Buffer::writeIdentifierArray(const std::vector<std::string>& ids) {
	writeVarInt((int)ids.size());
	for (const auto& id : ids)
		writeString(id);
}

// ============================================================
// ==========                UTILITAIRES               ==========
// ============================================================

std::vector<uint8_t>& Buffer::getData() { return _data; }
size_t Buffer::remaining() const { return _data.size() - _pos; }
