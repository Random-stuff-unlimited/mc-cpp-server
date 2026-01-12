#include "network/buffer.hpp"
#include "lib/UUID.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

Buffer::Buffer() : _pos(0) {}
Buffer::Buffer(const std::vector<uint8_t>& data) : _data(data), _pos(0) {}

static int countUtf16CodeUnits(const std::string& str) {
	int count = 0;
	for (size_t i = 0; i < str.size(); ++i) {
		unsigned char c = str[i];
		if ((c & 0x80) == 0x00) {
			// 1 byte
			count += 1;
		} else if ((c & 0xE0) == 0xC0) {
			// 2 bytes
			count += 1;
			i += 1;
		} else if ((c & 0xF0) == 0xE0) {
			// 3 bytes
			count += 1;
			i += 2;
		} else if ((c & 0xF8) == 0xF0) {
			// 4 bytes → surrogate pair
			count += 2;
			i += 3;
		} else {
			throw std::runtime_error("Invalid UTF-8 sequence");
		}
	}
	return count;
}


// ============================================================
// ==========           MÉTHODES DE LECTURE          ==========
// ============================================================

// Primitives (Big Endian)

bool Buffer::readBool() {
	return readUByte() != 0;
}

int8_t Buffer::readByte() {
	if (_pos >= _data.size()) throw std::runtime_error("Buffer underflow on byte");
	return static_cast<int8_t>(_data[_pos++]);
}

uint8_t Buffer::readUByte() {
	if (_pos >= _data.size()) throw std::runtime_error("Buffer underflow on byte");
	return _data[_pos++];
}

int16_t Buffer::readShort() {
	uint8_t byte1 = readUByte();
	uint8_t byte2 = readUByte();
	return static_cast<int16_t>
((byte1 << 8) | byte2);
}

uint16_t Buffer::readUShort() {
	return static_cast<uint16_t>((readUByte() << 8) | readUByte());
}

int32_t Buffer::readInt() {
	int32_t value = 0;
	for (int i = 0; i < 4; ++i)
		value = (value << 8) | readUByte();
	return value;
}

int64_t Buffer::readLong() {
	int64_t value = 0;
	for (int i = 0; i < 8; ++i)
		value = (value << 8) | readUByte();
	return value;
}

float Buffer::readFloat() {
	uint32_t bits = 0;
	for (int i = 0; i < 4; ++i)
		bits = (bits << 8) | readUByte();
	union { float f; uint32_t i; } u;
	u.i = bits;
	return u.f;
}

double Buffer::readDouble() {
	uint64_t bits = 0;
	for (int i = 0; i < 8; ++i)
		bits = (bits << 8) | readUByte();
	union { double d; uint64_t i; } u;
	u.i = bits;
	return u.d;
}

// Variable length

int32_t Buffer::readVarInt() {
	int32_t value = 0;
	int position = 0;
	uint8_t currentByte;

	do {
		currentByte = readUByte();
		value |= (currentByte & 0x7F) << position;
		position += 7;
		if (position >= 32) throw std::runtime_error("VarInt too big");
	} while (currentByte & 0x80);

	return value;
}

int64_t Buffer::readVarLong() {
	int64_t value = 0;
	int position = 0;
	uint8_t currentByte;

	do {
		currentByte = readUByte();
		value |= (static_cast<int64_t>(currentByte & 0x7F)) << position;
		position += 7;
		if (position >= 64) throw std::runtime_error("VarLong too big");
	} while (currentByte & 0x80);

	return value;
}

// Complex types

std::string Buffer::readString(int32_t maxLength) {
	int32_t byteLength = readVarInt();

	if (byteLength < 0)
		throw std::runtime_error("Negative string length");

	if (_pos + byteLength > _data.size())
		throw std::runtime_error("Buffer underflow on string");

	std::string result(reinterpret_cast<char*>(&_data[_pos]), byteLength);
	_pos += byteLength;

	int utf16Length = countUtf16CodeUnits(result);

	if (maxLength > 0 && utf16Length > maxLength)
		throw std::runtime_error("String exceeds maximum UTF-16 length");

	if (byteLength > utf16Length * 3)
		throw std::runtime_error("String exceeds maximum UTF-8 length");

	return result;
}

UUID Buffer::readUUID() {
	uint64_t msb = 0;
	uint64_t lsb = 0;

	for (int i = 0; i < 8; ++i)
		msb = (msb << 8) | readUByte();

	for (int i = 0; i < 8; ++i)
		lsb = (lsb << 8) | readUByte();

	return UUID(msb, lsb);
}

void Buffer::readPosition(int32_t& x, int32_t& y, int32_t& z) {
	int64_t packed = readLong();

	x = packed >> 38;
	z = (packed >> 12) << 38 >> 38;
	y = packed << 52 >> 52;
}

uint8_t Buffer::readAngle() {
	return readUByte();
}

// Arrays

std::vector<uint8_t> Buffer::readBytes(size_t length) {
	std::vector<uint8_t> data;
	data.reserve(length);
	for (size_t i = 0; i < length; ++i)
		data.push_back(readUByte());
	return data;
}

std::vector<int64_t> Buffer::readBitSet() {
	int32_t length = readVarInt();
	if (length < 0)
		throw std::runtime_error("Negative BitSet length");

	std::vector<int64_t> bitset;
	bitset.reserve(length);
	for (int32_t i = 0; i < length; ++i)
		bitset.push_back(readLong());

	return bitset;
}

std::vector<uint8_t> Buffer::readFixedBitSet(size_t numBits) {
	size_t numBytes = (numBits + 7) / 8;
	std::vector<uint8_t> bitset;
	bitset.reserve(numBytes);
	for (size_t i = 0; i < numBytes; ++i)
		bitset.push_back(readUByte());
	return bitset;
}

std::vector<std::string> Buffer::readStringArray() {
	int32_t count = readVarInt();
	if (count < 0) throw std::runtime_error("Negative array length");

	std::vector<std::string> result;
	result.reserve(count);
	for (int32_t i = 0; i < count; ++i)
		result.push_back(readString());
	return result;
}

std::vector<int32_t> Buffer::readVarIntArray() {
	int32_t count = readVarInt();
	if (count < 0) throw std::runtime_error("Negative array length");

	std::vector<int32_t> result;
	result.reserve(count);
	for (int32_t i = 0; i < count; ++i)
		result.push_back(readVarInt());
	return result;
}

// Legacy compatibility

uint64_t Buffer::readUInt64() {
	uint64_t value = 0;
	for (int i = 0; i < 8; ++i)
		value = (value << 8) | readUByte();
	return value;
}

int64_t Buffer::readVarInt64() {
	return readVarLong();
}

int64_t Buffer::readInt64() {
	return readLong();
}


// ============================================================
// ==========           MÉTHODES D'ÉCRITURE          ==========
// ============================================================

// Primitives (Big Endian)

void Buffer::writeBool(bool value) {
	writeUByte(value ? 1 : 0);
}

void Buffer::writeByte(int8_t value) {
	_data.push_back(static_cast<uint8_t>(value));
}

void Buffer::writeUByte(uint8_t value) {
	_data.push_back(value);
}

void Buffer::writeShort(int16_t value) {
	writeUByte((value >> 8) & 0xFF);
	writeUByte(value & 0xFF);
}

void Buffer::writeUShort(uint16_t value) {
	writeUByte((value >> 8) & 0xFF);
	writeUByte(value & 0xFF);
}

void Buffer::writeInt(int32_t value) {
	for (int i = 3; i >= 0; --i)
		writeUByte((value >> (i * 8)) & 0xFF);
}

void Buffer::writeLong(int64_t value) {
	for (int i = 7; i >= 0; --i)
		writeUByte((value >> (i * 8)) & 0xFF);
}

void Buffer::writeFloat(float value) {
	union { float f; uint32_t i; } u;
	u.f = value;
	for (int i = 3; i >= 0; --i)
		writeUByte((u.i >> (i * 8)) & 0xFF);
}

void Buffer::writeDouble(double value) {
	union { double d; uint64_t i; } u;
	u.d = value;
	for (int i = 7; i >= 0; --i)
		writeUByte((u.i >> (i * 8)) & 0xFF);
}

// Variable length

void Buffer::writeVarInt(int32_t value) {
	while (true) {
		if ((value & ~0x7F) == 0) {
			writeUByte(static_cast<uint8_t>(value));
			return;
		}
		writeUByte(static_cast<uint8_t>((value & 0x7F) | 0x80));
		value = static_cast<uint32_t>(value) >> 7;
	}
}

void Buffer::writeVarLong(int64_t value) {
	while (true) {
		if ((value & ~0x7FL) == 0) {
			writeUByte(static_cast<uint8_t>(value));
			return;
		}
		writeUByte(static_cast<uint8_t>((value & 0x7F) | 0x80));
		value = static_cast<uint64_t>(value) >> 7;
	}
}

// Complex types

void Buffer::writeString(const std::string& str) {
	int utf16Length = countUtf16CodeUnits(str);

	if (utf16Length > 32767)
		throw std::runtime_error("String too long (UTF-16 length)");

	if (str.size() > static_cast<size_t>(utf16Length * 3))
		throw std::runtime_error("String too long (UTF-8 length)");

	writeVarInt(static_cast<int32_t>(str.size()));
	writeBytes(str);
}


void Buffer::writeUUID(const UUID& uuid) {
	uint64_t msb = uuid.getMostSigBits();
	uint64_t lsb = uuid.getLeastSigBits();

	for (int i = 7; i >= 0; --i)
		writeUByte((msb >> (i * 8)) & 0xFF);

	for (int i = 7; i >= 0; --i)
		writeUByte((lsb >> (i * 8)) & 0xFF);
}

void Buffer::writePosition(int32_t x, int32_t y, int32_t z) {
	int64_t packed = ((static_cast<int64_t>(x) & 0x3FFFFFF) << 38) |
	                 ((static_cast<int64_t>(z) & 0x3FFFFFF) << 12) |
	                 (static_cast<int64_t>(y) & 0xFFF);
	writeLong(packed);
}

void Buffer::writeAngle(uint8_t value) {
	writeUByte(value);
}

// Arrays

void Buffer::writeBytes(const std::vector<uint8_t>& data) {
	_data.insert(_data.end(), data.begin(), data.end());
}

void Buffer::writeBytes(const uint8_t* data, size_t length) {
	for (size_t i = 0; i < length; ++i)
		writeUByte(data[i]);
}

void Buffer::writeBitSet(const std::vector<int64_t>& bitset) {
	writeVarInt(static_cast<int32_t>(bitset.size()));
	for (int64_t value : bitset)
		writeLong(value);
}

void Buffer::writeFixedBitSet(const std::vector<uint8_t>& bitset, size_t numBits) {
	size_t expectedBytes = (numBits + 7) / 8;
	if (bitset.size() != expectedBytes)
		throw std::runtime_error("FixedBitSet size mismatch");

	for (uint8_t byte : bitset)
		writeUByte(byte);
}

void Buffer::writeIdentifierArray(const std::vector<std::string>& ids) {
	writeVarInt(static_cast<int32_t>(ids.size()));
	for (const auto& id : ids)
		writeString(id);
}

// Legacy/utility methods

void Buffer::writeBytes(const std::string& data) {
	_data.insert(_data.end(), data.begin(), data.end());
}

void Buffer::writeUInt(uint32_t value) {
	for (int i = 3; i >= 0; --i)
		writeUByte((value >> (i * 8)) & 0xFF);
}

void Buffer::writeInt64(int64_t value) {
	writeLong(value);
}

void Buffer::writeVarInt64(int64_t value) {
	writeVarLong(value);
}

void Buffer::prependByte(uint8_t byte) {
	_data.insert(_data.begin(), byte);
}

void Buffer::prependBytes(const std::string& data) {
	_data.insert(_data.begin(), data.begin(), data.end());
}

void Buffer::prependVarInt(int value) {
	std::vector<uint8_t> tmp;
	uint32_t v = static_cast<uint32_t>(value);

	while (true) {
		if ((v & ~0x7F) == 0) {
			tmp.push_back(static_cast<uint8_t>(v));
			break;
		}
		tmp.push_back(static_cast<uint8_t>((v & 0x7F) | 0x80));
		v >>= 7;
	}
	_data.insert(_data.begin(), tmp.begin(), tmp.end());
}


// ============================================================
// ==========                UTILITAIRES               ==========
// ============================================================

std::vector<uint8_t>& Buffer::getData() { return _data; }
size_t Buffer::remaining() const { return _data.size() - _pos; }
