#ifndef PACKET_HPP
# define PACKET_HPP

# include "server.hpp"

class Packet {
	private:
		int32_t	_size;
		int32_t	_id;
		Buffer	_data;
		Player	*_player;
		int		_socket_fd;

	public:
   		Packet() = default;
		static int readVarint(int sock);
		static void writeVarint(int sock, int value);
		static int varintLen(int value);
};

#endif