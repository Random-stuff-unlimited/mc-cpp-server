#ifndef BUFFER_HPP
#define BUFFER_HPP

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
};

#endif
