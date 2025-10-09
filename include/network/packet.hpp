#ifndef PACKET_HPP
#define PACKET_HPP

#include "../player.hpp"
#include "buffer.hpp"
#include "server.hpp"

#include <cstdint>
#include <string>

enum PacketResult { PACKET_OK = 0, PACKET_SEND = 1, PACKET_DISCONNECT = 2, PACKET_ERROR = -1 };

class Packet {
  private:
	int32_t _size;
	int32_t _id;
	Buffer	_data;
	Player* _player;
	int		_socketFd;
	int		_returnPacket;

  public:
	Packet(Player* player);
	Packet(int socketFd, Server& server);
	Packet(const Packet& other);
	Packet& operator=(const Packet& other);
	~Packet();
	static int	readVarint(int sock);
	static int	readVarint(int sock, int* bytesRead);
	static void writeVarint(int sock, int value);
	static int	varintLen(int value);
	static bool isSocketValid(int sock);
	Player*		getPlayer() const;
	uint32_t	getSize();
	uint32_t	getId();
	Buffer&		getData();
	int			getSocket() const;
	void		setReturnPacket(int value);
	int			getReturnPacket();
	int			getVarintSize(int32_t value);
	void		setPacketSize(int32_t value);
	void		setPacketId(uint32_t value);
};

#endif
