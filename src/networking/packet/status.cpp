
// void Packet::handleStatusRequest(int clientSock, Server &server) {
// 	int requestLen = readVarint(clientSock);
// 	if (requestLen <= 0 || requestLen > 2048)
// 		return;

// 	std::vector<uint8_t> buffer(requestLen);
// 	ssize_t bytesRead = ::read(clientSock, buffer.data(), requestLen);
// 	if (bytesRead != requestLen)
// 		return;

// 	Buffer buf(buffer);
// 	int packetId = buf.readVarInt();
// 	if (packetId != 0x00)
// 		return;

// 	json jres = {
// 		{"version", {{"name", server.getGameVersion()}, {"protocol", server.getProtocolVersion()}}},
// 		{"players", {{"max", server.getServerSize()}, {"online", server.getAmountOnline()}, {"sample", server.getPlayerSample()}}},
// 		{"description", {{"text", server.getServerMOTD()}}}
// 	};

// 	std::string payload = jres.dump();
// 	int jsonLen = payload.size();
// 	int totalLen = varintLen(0x00) + varintLen(jsonLen) + jsonLen;

// 	writeVarint(clientSock, totalLen);
// 	writeVarint(clientSock, 0x00);
// 	writeVarint(clientSock, jsonLen);
// 	::write(clientSock, payload.data(), jsonLen);

// 	std::cout << "[Status] Sent JSON response\n";

// 	int pingLen = readVarint(clientSock);
// 	if (pingLen <= 0)
// 		return;

// 	std::vector<uint8_t> pingBuffer(pingLen);
// 	bytesRead = ::read(clientSock, pingBuffer.data(), pingLen);
// 	if (bytesRead != pingLen) return;

// 	Buffer pingBuf(pingBuffer);
// 	int pingId = pingBuf.readVarInt();
// 	if (pingId != 0x01) return;

// 	uint64_t pingPayload = pingBuf.readUInt64();

// 	int pongLen = varintLen(0x01) + 8; // 1 byte PacketID + 8 bytes payload
// 	writeVarint(clientSock, pongLen);
// 	writeVarint(clientSock, 0x01);
// 	::write(clientSock, &pingPayload, sizeof(pingPayload));

// 	std::cout << "[Status] Pong sent\n";
// }
