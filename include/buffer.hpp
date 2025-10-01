#ifndef BUFFER_HPP
# define BUFFER_HPP

#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>

class Buffer {
	private:
		std::vector<uint8_t> _data;
		size_t _pos;

	public:
		Buffer();
		explicit Buffer(const std::vector<uint8_t> &data);

		int readVarInt();
		void writeVarInt(int value);

		std::string readString();
		void writeString(const std::string &str);

		const std::vector<uint8_t>& getData() const;
		size_t remaining() const;
		uint16_t readUShort();
		uint64_t readUInt64();
};


#endif