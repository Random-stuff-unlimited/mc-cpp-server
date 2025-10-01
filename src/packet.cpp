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
	return len;
}

int Packet::handleHandshake(int clientSock) {
	int handshakeLen = readVarint(clientSock);
	if (handshakeLen <= 0 || handshakeLen > 2048) {
		return (1);
	}

	std::vector<uint8_t> buffer(handshakeLen);
	int bytesRead = ::read(clientSock, buffer.data(), handshakeLen);
	if (bytesRead != handshakeLen) {
		return -1;
	}

	Buffer buf(buffer);
	int packetId = buf.readVarInt();
	if (packetId != 0x00) return -1;

	int protocolVersion = buf.readVarInt();
	std::string serverAddr = buf.readString();
	uint16_t port = buf.readUShort();
	int nextState = buf.readVarInt();

	std::cout << "[Handshake] Protocol=" << protocolVersion
			<< ", Addr=" << serverAddr
			<< ", State=" << nextState << "\n";

	return (nextState == 1 || nextState == 2) ? nextState : -1;
}

void Packet::handleStatusRequest(int clientSock, Server &server) {
	int requestLen = readVarint(clientSock);
	if (requestLen <= 0 || requestLen > 2048)
		return;

	std::vector<uint8_t> buffer(requestLen);
	ssize_t bytesRead = ::read(clientSock, buffer.data(), requestLen);
	if (bytesRead != requestLen)
		return;

	Buffer buf(buffer);
	int packetId = buf.readVarInt();
	if (packetId != 0x00)
		return;

	json jres = {
		{"version", {{"name", server.getGameVersion()}, {"protocol", server.getProtocolVersion()}}},
		{"players", {{"max", server.getServerSize()}, {"online", server.getAmountOnline()}, {"sample", server.getPlayerSample()}}},
		{"description", {{"text", server.getServerMOTD()}}}
	};

	std::string payload = jres.dump();
	int jsonLen = payload.size();
	int totalLen = varintLen(0x00) + varintLen(jsonLen) + jsonLen;

	writeVarint(clientSock, totalLen);
	writeVarint(clientSock, 0x00);
	writeVarint(clientSock, jsonLen);
	::write(clientSock, payload.data(), jsonLen);

	std::cout << "[Status] Sent JSON response\n";

	int pingLen = readVarint(clientSock);
	if (pingLen <= 0)
		return;

	std::vector<uint8_t> pingBuffer(pingLen);
	bytesRead = ::read(clientSock, pingBuffer.data(), pingLen);
	if (bytesRead != pingLen) return;

	Buffer pingBuf(pingBuffer);
	int pingId = pingBuf.readVarInt();
	if (pingId != 0x01) return;

	uint64_t pingPayload = pingBuf.readUInt64();

	int pongLen = varintLen(0x01) + 8; // 1 byte PacketID + 8 bytes payload
	writeVarint(clientSock, pongLen);
	writeVarint(clientSock, 0x01);
	::write(clientSock, &pingPayload, sizeof(pingPayload));

	std::cout << "[Status] Pong sent\n";
}

void Packet::sendDisconnectMessage(int clientSock, const std::string &reason) {
	json jres = {{"text", reason}, {"color", "red"}};
	std::string payload = jres.dump();
	int jsonLen = payload.size();
	int totalLen = varintLen(0x00) + varintLen(jsonLen) + jsonLen;

	writeVarint(clientSock, totalLen);
	writeVarint(clientSock, 0x00);
	writeVarint(clientSock, jsonLen);
	::write(clientSock, payload.data(), jsonLen);

	std::cout << "[Disconnect] " << reason << "\n";
}
