
// int	Packet::handleLoginStart(int clientSock, const Server &server) {
// 	int requestLen = readVarint(clientSock);
// 	if (requestLen <= 0 || requestLen > 2048)
// 		return;
	
// 	std::vector<uint8_t> buffer(requestLen);
// 	ssize_t bytesRead = ::read(clientSock, buffer.data(), requestLen);
// 	if (bytesRead != requestLen)
// 		return ;

// 	Buffer buf(buffer);

// 	int packetId = buf.readVarInt();
// 	if (packetId != 0x00)
// 		return ;
	
// 	std::string playerName = buf.readString(16);

// 	UUID playerUUID;
// 	if (buf.remaining() >= 16) {
// 		playerUUID = buf.readUUID();
// 	} else {
// 		playerUUID = UUID(); // ou laisser vide / nul
// 	}
// }