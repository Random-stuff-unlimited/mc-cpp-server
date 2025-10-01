#include "packet.hpp"
#include "buffer.hpp"
#include <unistd.h>
#include <iostream>
#include <vector>
#include "json.hpp"

using json = nlohmann::json;

int Packet::readVarint(int sock) {
	int value = 0, position = 0;
	uint8_t byte;
	while (true) {
		if (::read(sock, &byte, 1) <= 0) return -1;
		value |= (byte & 0x7F) << position;
		if (!(byte & 0x80)) break;
		position += 7;
		if (position >= 32) return -1;
	}
	return value;
}

void Packet::writeVarint(int sock, int value) {
	std::vector<uint8_t> tmp;
	Buffer buf(tmp);
	buf.writeVarInt(value);
	::write(sock, buf.getData().data(), buf.getData().size());
}

int Packet::varintLen(int value) {
	int len = 0;
	do {
		len++;
		value >>= 7;
	} while (value != 0);
	return (len);
}
