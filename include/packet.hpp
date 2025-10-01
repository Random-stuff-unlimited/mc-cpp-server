#ifndef PACKET_HPP
# define PACKET_HPP

# include "server.hpp"

class Packet {
	public:
   		Packet() = default;
		static int readVarint(int sock);
		static void writeVarint(int sock, int value);
		static int varintLen(int value);

		static int handleHandshake(int clientSock);
		static void handleStatusRequest(int clientSock, Server &server);
		static void sendDisconnectMessage(int clientSock, const std::string &reason);
};

#endif