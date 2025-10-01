
// void Packet::sendDisconnectMessage(int clientSock, const std::string &reason) {
// 	json jres = {{"text", reason}, {"color", "red"}};
// 	std::string payload = jres.dump();
// 	int jsonLen = payload.size();
// 	int totalLen = varintLen(0x00) + varintLen(jsonLen) + jsonLen;

// 	writeVarint(clientSock, totalLen);
// 	writeVarint(clientSock, 0x00);
// 	writeVarint(clientSock, jsonLen);
// 	::write(clientSock, payload.data(), jsonLen);

// 	std::cout << "[Disconnect] " << reason << "\n";
// }