// int Packet::handleHandshake(int clientSock, int serverPort) {
// 	int handshakeLen = readVarint(clientSock);
// 	if (handshakeLen <= 0 || handshakeLen > 2048) {
// 		return (1);
// 	}

// 	std::vector<uint8_t> buffer(handshakeLen);
// 	int bytesRead = ::read(clientSock, buffer.data(), handshakeLen);
// 	if (bytesRead != handshakeLen)
// 		return (1);

// 	Buffer buf(buffer);
// 	int packetId = buf.readVarInt();
// 	if (packetId != 0x00) return -1;

// 	int protocolVersion = buf.readVarInt();
// 	std::string serverAddr;
// 	try {
// 		serverAddr = buf.readString(16);
// 	} catch (const std::exception& e) {
// 		std::cerr << "[Handshake] Invalid server address (too long)\n";
// 		return (1);
// 	}

// 	uint16_t port = buf.readUShort();
// 	int nextState = buf.readVarInt();

// 	if (serverPort != port)
// 	{
// 		std::cerr << "[Handshake] Invalid server port (client / server different)\n";
// 		return (1);
// 	}

// 	std::cout << "[Handshake] Protocol=" << protocolVersion
// 			<< ", Addr=" << serverAddr
// 			<< ", State=" << nextState << "\n";

// 	return (nextState == 1 || nextState == 2) ? nextState : -1;
// }