#ifndef PACKET_HPP
# define PACKET_HPP

# include "server.hpp"
# include "buffer.hpp"
# include "player.hpp"
# include <cstdint>

class Packet {
	private:
		int32_t			_size;
		int32_t			_id;
		Buffer			_data;
		Player			*_player;
		int				_socketFd;
		int				_returnPacket;

	public:
		Packet(Player *player);
		Packet(int socketFd, Server &server);
        static int readVarint(int sock);
        static void writeVarint(int sock, int value);
		static int varintLen(int value);
		Player *getPlayer() const;
		uint32_t getSize();
		uint32_t getId();
		Buffer	&getData();
		int		getSocket() const;
		void	setReturnPacket(int value);
		int		getReturnPacket();
		int     getVarintSize(int32_t value);
};

#endif
