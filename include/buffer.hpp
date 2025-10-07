#ifndef BUFFER_HPP
#define BUFFER_HPP

#include "UUID.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class Buffer {
  private:
	std::vector<uint8_t> _data;
	size_t _pos;

  public:
	Buffer();
	explicit Buffer(const std::vector<uint8_t>& data);

	int readVarInt();
	void writeVarInt(int value);
	void writeInt(int32_t value);
	void writeIdentifierArray(const std::vector<std::string>& ids);
	void writeUInt(uint32_t value);

	std::string readString(int maxLength);
	void writeString(const std::string& str);

	std::vector<uint8_t>& getData();
	size_t remaining() const;
	uint16_t readUShort();
	uint64_t readUInt64();
	long readLong();
	void writeLong(long value);
	uint8_t readByte();
	void writeByte(uint8_t byte);
	void writeBytes(const std::string& data);
	void writeBytes(const std::vector<uint8_t>& data);
	void writeUUID(const UUID& uuid);

	void writeBool(bool value);
    void writeNBT(const std::string& nbtData);
    void writePosition(int32_t x, int32_t y, int32_t z);
    void writeFloat(float value);
    void writeDouble(double value);
    void writeIdentifier(const std::string& id);
    void writeVarLong(int64_t value);
};

#endif
