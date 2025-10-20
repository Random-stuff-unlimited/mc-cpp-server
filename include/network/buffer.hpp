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
	uint8_t   readByte();
	bool      readBool();
	int32_t   readInt();
	uint16_t  readUShort();
	uint64_t  readUInt64();
	int32_t   readVarInt();
	int64_t   readVarInt64();
	std::string readString(int maxLength);
	std::string readString();
    int64_t  readInt64();

	std::vector<std::string> readStringArray();
	std::vector<int>         readVarIntArray();

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
	void writeByte(uint8_t byte);
	void writeBytes(const std::string& data);
	void writeBytes(const std::vector<uint8_t>& data);

	void writeBool(bool value);
	void writeInt(int32_t value);
	void writeUInt(uint32_t value);
	void writeUShort(uint16_t value);
	void writeInt64(int64_t value);
	void writeFloat(float value);
	void writeDouble(double value);

	void writeVarInt(int value);
	void writeVarInt64(int64_t value);

	void writeString(const std::string& str);
	void writeUUID(const UUID& uuid);
	void writePosition(int32_t x, int32_t y, int32_t z);
	void writeIdentifierArray(const std::vector<std::string>& ids);
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
