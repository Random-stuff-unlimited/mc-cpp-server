#include "networking.hpp"
#include "packet.hpp"
#include <unistd.h>

void	handleStatusPacket(Packet &packet, Server &server)
{
	if (packet.getId() != 0x00)
	{
		packet.getPlayer()->setPlayerState(PlayerState::None);
		close(packet.getSocket());
		server.removePlayer(packet.getPlayer());
	}

}
