#ifndef PACKET_HPP
# define PACKET_HPP

# include "server.hpp"
# include "buffer.hpp"

class Packet {
	private:
		int32_t			_size;
		int32_t			_id;
		Buffer			_data;
		const Player	*_player;
		int				_socketFd;

	public:
   		Packet(const Player *player);
		Packet(const int socketFd);
		static int readVarint(int sock);
		static void writeVarint(int sock, int value);
		static int varintLen(int value);
		const Player *getPlayer();
		uint32_t *Packet::getSize();
		uint32_t *Packet::getId();
		Buffer	&Packet::getData();
};

#endif