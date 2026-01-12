#ifndef BUFFER_HPP
#define BUFFER_HPP

#include "../lib/UUID.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

class Buffer {
  private:
	std::vector<uint8_t> _data;
	size_t               _pos;

  public:
	// ============================================================
	// ==========            CONSTRUCTEURS              ==========
	// ============================================================
	Buffer();
	explicit Buffer(const std::vector<uint8_t>& data);


	// ============================================================
	// ==========              LECTURE                 ==========
	// ============================================================
	
	// Primitives (Big Endian)
	bool      readBool();
	int8_t    readByte();
	uint8_t   readUByte();
	int16_t   readShort();
	uint16_t  readUShort();
	int32_t   readInt();
	int64_t   readLong();
	float     readFloat();
	double    readDouble();
	
	// Variable length
	int32_t   readVarInt();
	int64_t   readVarLong();
	
	// Complex types
	std::string readString(int32_t maxLength = 32767);
	UUID        readUUID();
	void        readPosition(int32_t& x, int32_t& y, int32_t& z);
	uint8_t     readAngle();
	
	// Arrays
	std::vector<uint8_t> readBytes(size_t length);
	std::vector<int64_t> readBitSet();
	std::vector<uint8_t> readFixedBitSet(size_t numBits);
	
	std::vector<std::string> readStringArray();
	std::vector<int32_t>     readVarIntArray();
	
	// Legacy compatibility
	uint64_t  readUInt64();
	int64_t   readVarInt64();
	int64_t   readInt64();

	template <typename T>
	std::vector<T> readArray(std::function<T()> reader) {
		int count = readVarInt();
		if (count < 0) throw std::runtime_error("Negative array length");
		std::vector<T> result;
		result.reserve(count);
		for (int i = 0; i < count; ++i)
			result.push_back(reader());
		return result;
	}


	// ============================================================
	// ==========              ÉCRITURE                ==========
	// ============================================================
	
	// Primitives (Big Endian)
	void writeBool(bool value);
	void writeByte(int8_t value);
	void writeUByte(uint8_t value);
	void writeShort(int16_t value);
	void writeUShort(uint16_t value);
	void writeInt(int32_t value);
	void writeLong(int64_t value);
	void writeFloat(float value);
	void writeDouble(double value);
	
	// Variable length
	void writeVarInt(int32_t value);
	void writeVarLong(int64_t value);
	
	// Complex types
	void writeString(const std::string& str);
	void writeUUID(const UUID& uuid);
	void writePosition(int32_t x, int32_t y, int32_t z);
	void writeAngle(uint8_t value);
	
	// Arrays
	void writeBytes(const std::vector<uint8_t>& data);
	void writeBytes(const uint8_t* data, size_t length);
	void writeBitSet(const std::vector<int64_t>& bitset);
	void writeFixedBitSet(const std::vector<uint8_t>& bitset, size_t numBits);
	
	void writeIdentifierArray(const std::vector<std::string>& ids);
	
	// Legacy/utility methods
	void writeBytes(const std::string& data);
	void writeUInt(uint32_t value);
	void writeInt64(int64_t value);
	void writeVarInt64(int64_t value);
	void prependBytes(const std::string& data);
	void prependByte(uint8_t byte);
	void prependVarInt(int value);

	// ============================================================
	// ==========              UTILITAIRES             ==========
	// ============================================================
	std::vector<uint8_t>& getData();
	size_t                remaining() const;
};

#endif
