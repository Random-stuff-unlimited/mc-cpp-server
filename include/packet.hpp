#ifndef PACKET_HPP
#define PACKET_HPP

#include "buffer.hpp"
#include "player.hpp"
#include "server.hpp"

#include <cstdint>

enum PacketResult { PACKET_OK = 0, PACKET_SEND = 1, PACKET_DISCONNECT = 2, PACKET_ERROR = -1 };

class Packet {
  private:
	int32_t _size;
	int32_t _id;
	Buffer _data;
	Player* _player;
	int _socketFd;
	int _returnPacket;

  public:
	Packet(Player* player);
	Packet(int socketFd, Server& server);
	~Packet();
	static int readVarint(int sock);
	static void writeVarint(int sock, int value);
	static int varintLen(int value);
	static bool isSocketValid(int sock);
	Player* getPlayer() const;
	uint32_t getSize();
	uint32_t getId();
	Buffer& getData();
	int getSocket() const;
	void setReturnPacket(int value);
	int getReturnPacket();
	int getVarintSize(int32_t value);
	void setPacketSize(int32_t value);
	void setPacketId(int32_t value);
};

#endif
