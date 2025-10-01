#ifndef BUFFER_HPP
# define BUFFER_HPP

#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>
#include "UUID.hpp"

class Buffer {
	private:
		std::vector<uint8_t> data;
		size_t pos;

	public:
		Buffer();
		explicit Buffer(const std::vector<uint8_t> &data);

		int readVarInt();
		void writeVarInt(int value);

		std::string readString(int maxLength = -1);
		void writeString(const std::string &str);

		const std::vector<uint8_t>& getData() const;
		size_t remaining() const;
		uint8_t readByte(); 
		void	writeByte(uint8_t b);
		UUID 	readUUID();
		uint16_t readUShort();
		uint64_t readUInt64();
};


#endif