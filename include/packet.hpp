#ifndef PACKET_HPP
# define PACKET_HPP

# include "server.hpp"

class Packet
{
	private:
		int readVarint(int sock);
		void writeVarint(int sock, int value);
		static int readVarintFromBuffer(char **buffer);
		static void readStringFromBuffer(char **buffer, char *dest, int max_len);
		static int varintLen(int value);
	public:
		int handleHandshake(int client_sock);
		void handleStatusRequest(int client_sock, Server &server);
		void sendDisconnectMessage(int client_sock, std::string reason);
		//void addPlayer(t_server *server, int client_sock, const char *player_name);
		//int handleLoginStart(int client_sock, t_server *server);
		Packet();
		~Packet();
};

#endif