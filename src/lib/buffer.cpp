#include "network/buffer.hpp"

#include "lib/UUID.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

Buffer::Buffer() : _pos(0) {}
Buffer::Buffer(const std::vector<uint8_t>& data) : _data(data), _pos(0) {}

uint8_t Buffer::readByte() {
	if (_pos >= _data.size()) throw std::runtime_error("Buffer underflow on byte");
	return _data[_pos++];
}

void Buffer::writeByte(uint8_t byte) { _data.push_back(byte); }

void Buffer::writeBytes(const std::string& data) { _data.insert(_data.end(), data.begin(), data.end()); }

void Buffer::writeBytes(const std::vector<uint8_t>& data) { _data.insert(_data.end(), data.begin(), data.end()); }

void Buffer::writeUUID(const UUID& uuid) {
	uint64_t msb = uuid.getMostSigBits();
	uint64_t lsb = uuid.getLeastSigBits();
	for (int i = 7; i >= 0; --i)
		writeByte((msb >> (i * 8)) & 0xFF);
	for (int i = 7; i >= 0; --i)
		writeByte((lsb >> (i * 8)) & 0xFF);
}

int Buffer::readVarInt() {
	int		value = 0, position = 0;
	uint8_t currentByte;

	do {
		currentByte = readByte();
		value |= (currentByte & 0x7F) << position;
		position += 7;
		if (position >= 32) throw std::runtime_error("VarInt too big");
	} while (currentByte & 0x80);

	return value;
}

void Buffer::writeInt(int32_t value) {
	writeByte(static_cast<uint8_t>((value >> 24) & 0xFF));
	writeByte(static_cast<uint8_t>((value >> 16) & 0xFF));
	writeByte(static_cast<uint8_t>((value >> 8) & 0xFF));
	writeByte(static_cast<uint8_t>(value & 0xFF));
}

void Buffer::writeVarInt(int value) {
	while (true) {
		if ((value & ~0x7F) == 0) {
			writeByte(static_cast<uint8_t>(value));
			return;
		} else {
			writeByte(static_cast<uint8_t>((value & 0x7F) | 0x80));
			value >>= 7;
		}
	}
}

void Buffer::writeUInt(uint32_t value) {
	writeByte(static_cast<uint8_t>((value >> 24) & 0xFF));
	writeByte(static_cast<uint8_t>((value >> 16) & 0xFF));
	writeByte(static_cast<uint8_t>((value >> 8) & 0xFF));
	writeByte(static_cast<uint8_t>(value & 0xFF));
}

void Buffer::writeIdentifierArray(const std::vector<std::string>& ids) {
	writeVarInt(static_cast<int>(ids.size()));
	for (const auto& id : ids) {
		writeString(id);
	}
}

std::string Buffer::readString(int maxLength) {
	int len = readVarInt();

	if (maxLength > 0 && len > maxLength) {
		throw std::runtime_error("String length exceeds maximum allowed");
	}

	if (_pos + len > _data.size()) {
		throw std::runtime_error("Buffer underflow on string");
	}

	std::string result(reinterpret_cast<char*>(&_data[_pos]), len);
	_pos += len;
	return result;
}

void Buffer::writeString(const std::string& str) {
	writeVarInt(static_cast<int>(str.size()));
	_data.insert(_data.end(), str.begin(), str.end());
}

std::vector<uint8_t>& Buffer::getData() { return _data; }

size_t Buffer::remaining() const { return _data.size() - _pos; }

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

long Buffer::readLong() {
	long value = 0;
	for (int i = 0; i < 8; ++i) {
		value = (value << 8) | readByte();
	}
	return value;
}

void Buffer::writeLong(long value) {
	for (int i = 7; i >= 0; --i) {
		writeByte(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
	}
}

void Buffer::writeBool(bool value) { writeByte(value ? 0x01 : 0x00); }

void Buffer::writeNBT(const std::string& nbtData) {
	// Format anonymousNbt pour MC 1.21.5 - compound NBT minimal mais valide
	if (nbtData == "{}") {
		// Compound NBT vide mais valide:
		// TAG_String + nom vide + valeur vide + TAG_End
		writeByte(0x08); // TAG_String
		writeVarInt(0);	 // Nom de longueur 0 (anonyme)
		writeVarInt(0);	 // Valeur string vide
		writeByte(0x00); // TAG_End pour fermer le compound
	} else {
		// Pour autres données NBT, utiliser la même structure minimale
		writeByte(0x08); // TAG_String
		writeVarInt(0);	 // Nom de longueur 0
		writeVarInt(0);	 // Valeur string vide
		writeByte(0x00); // TAG_End
	}
}

void Buffer::writePosition(int32_t x, int32_t y, int32_t z) {
	int64_t packed = ((int64_t)(x & 0x3FFFFFF) << 38) | ((int64_t)(y & 0xFFF) << 26) | (int64_t)(z & 0x3FFFFFF);
	writeLong(packed);
}

void Buffer::writeFloat(float value) {
	union {
		float	 f;
		uint32_t i;
	} u;
	u.f = value;
	writeUInt(u.i);
}

void Buffer::writeDouble(double value) {
	union {
		double	 d;
		uint64_t i;
	} u;
	u.d = value;
	writeLong(u.i);
}

void Buffer::writeVarLong(int64_t value) {
	while (true) {
		if ((value & ~0x7FL) == 0) {
			writeByte((uint8_t)value);
			return;
		} else {
			writeByte((uint8_t)((value & 0x7F) | 0x80));
			value >>= 7;
		}
	}
}

void Buffer::writeIdentifier(const std::string& id) { writeString(id); }
