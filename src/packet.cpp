#include "packet.hpp"
#include <iostream>
#include <unistd.h>
#include <cstring>

int Packet::readVarint(int sock) {
	int value = 0, position = 0;
	unsigned char byte;
	while (1) {
		if (read(sock, &byte, 1) <= 0) return -1;
		value |= (byte & 0x7F) << position;
		if (!(byte & 0x80)) break;
		position += 7;
		if (position >= 32) return -1;
	}
	return value;
}

void Packet::writeVarint(int sock, int value) {
	while (1) {
		if ((value & ~0x7f) == 0) {
			unsigned char b = value;
			write(sock, &b, 1);
			return;
		}
		unsigned char b = (value & 0x7F) | 0x80;
		write(sock, &b, 1);
		value >>= 7;
	}
}

int Packet::readVarintFromBuffer(char **buffer) {
	int value = 0;
	int position = 0;
	unsigned char currentByte;
	char *ptr = *buffer;
	while (1) {
		currentByte = *ptr;
		ptr++;
		value |= (currentByte & 0x7F) << position;
		if ((currentByte & 0x80) == 0) break;
		position += 7;
	}
	*buffer = ptr;
	return value;
}

void Packet::readStringFromBuffer(char **buffer, char *dest, int maxLen) {
	int len = Packet::readVarintFromBuffer(buffer);
	if (len > maxLen - 1) len = maxLen -1;
	memcpy(dest, *buffer, len);
	dest[len] = '\0';
	*buffer += len;
}

int Packet::varintLen(int value) {
	int len = 0;
	do {
		len++;
		value >>= 7;
	} while (value != 0);
	return len;
}

int Packet::handleHandshake(int client_sock) {
	std::cout << "Handshake process..." << std::endl;
	char packetBuffer[2048];

	int handshakeLen = readVarint(client_sock);

	if (handshakeLen <= 0 || handshakeLen > (int)sizeof(packetBuffer)) {
		std::cerr << "Error: Invalid handshake length or client disconnected (" << handshakeLen << " bytes)." << std::endl;
		return -1;
	}

	int bytesRead = 0;
	while (bytesRead < handshakeLen)
	{
		int result = read(client_sock, packetBuffer + bytesRead, handshakeLen - bytesRead);
		if (result <= 0)
		{
			perror("read handshake data");
			std::cerr << "Error: Unable to read handshake data." << std::endl;
			return -1;
		}
		bytesRead += result;
	}
	std::cout << "Handshake received (length: " << handshakeLen << ")" << std::endl;

	char *bufferPtr = packetBuffer;

	int packetId = Packet::readVarintFromBuffer(&bufferPtr);
	if (packetId != 0x00)
	{
		std::cerr << "Error: Unexpected handshake packet ID: 0x" << packetId << std::endl;
		return -1;
	}

	int protocolVersion = readVarintFromBuffer(&bufferPtr);

	char serverAddr[256];
	Packet::readStringFromBuffer(&bufferPtr, serverAddr, sizeof(serverAddr));

	bufferPtr += 2;

	int nextState = readVarintFromBuffer(&bufferPtr);

	std::cout << "Handshake analyzed: Protocol Version=" << protocolVersion << ", Address='" << serverAddr 
				<< "', Requested State" << nextState << std::endl;

	if (nextState == 1 || nextState == 2) 
		return nextState;
	
	std::cerr << "Error: Unknown next state requested: " << nextState << std::endl;
	return -1;
}