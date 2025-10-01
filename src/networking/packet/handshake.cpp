#include "server.hpp"
#include "packet.hpp"
#include <unistd.h>

void	handleHandshakePacket(Packet &packet, Server &server)
{
	if (packet.getId() != 0x00) // not handshake packet
		return ;
	int protocolVersion = packet.getData().readVarInt();
	std::string serverAddr = packet.getData().readString(255);
	uint16_t port = packet.getData().readUShort();
	int nextState = packet.getData().readVarInt();
	std::cout << "[Handshake] Protocol=" << protocolVersion
		<< ", Addr=" << serverAddr
		<< ", State=" << nextState << "\n";
	if (nextState == 1)
		packet.getPlayer()->setPlayerState(PlayerState::Status);
	else if (nextState == 2)
		packet.getPlayer()->setPlayerState(PlayerState::Login);
	else
	{
		packet.getPlayer()->setPlayerState(PlayerState::None);
		close(packet.getSocket());
		server.removePlayer(packet.getPlayer());
	}
}
