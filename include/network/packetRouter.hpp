#ifndef PACKET_ROUTER_HPP
# define PACKET_ROUTER_HPP

# include "packet.hpp"
# include "server.hpp"

// clientbound
void changeDifficultyPacket(Packet& packet, Server& server);
void clientboundKnownPacksPacket(Packet& packet, Server& server);
void gameEventPacket(Packet& packet, Server& server);
void handleFinishConfigurationPacket(Packet& packet, Server& server);
void handleLoginStartPacket(Packet& packet, Server& server);
void handlePingPacket(Packet& packet, Server& server);
void handleStatusPacket(Packet& packet, Server& server);
void playerAbilitiesPacket(Packet& packet, Server& server);
void sendPlayPacket(Packet& packet, Server& server);
void writeSpawnInfo(Buffer& buf, Player& player, Server& server);
void setHeldItemPacket(Packet& packet, Server& server);
void synchronizePlayerPositionPacket(Packet& packet, Server& server);
void clientboundFeatureFlagsPacket(Packet& packet, Server& server);

// serverbound
void handleAcknowledgeFinishConfigurationPacket(Packet& packet, Server& server);
void handleClientInformationPacket(Packet& packet, Server& server);
void handleConfirmTeleportationPacket(Packet& packet, Server& server);
void handleHandshakePacket(Packet& packet, Server& server);
void handleLoginAcknowledgedPacket(Packet& packet, Server& server);
void serverboundKnownPacksPacket(Packet& packet);
void handlePlayerActionPacket(Packet& packet, Server& server);
void handleUseItemOnPacket(Packet& packet, Server& server);
void handleSetCarriedItemPacket(Packet& packet, Server& server);
void handleSetCreativeModeSlotPacket(Packet& packet, Server& server);

// Network threads: Handshake, Status, Login and Configuration states
void packetRouter(Packet* packet, Server& server);
// Game thread: Play state
void playPacketRouter(Packet* packet, Server& server);
// Game thread: Finish Configuration acknowledged, the player enters the world
void enterPlay(Packet* packet, Server& server);

void handleHandshakeState(Packet* packet, Server& server);
void handleStatusState(Packet* packet, Server& server);
void handleLoginState(Packet* packet, Server& server);
void handleConfigurationState(Packet* packet, Server& server);
void handlePlayState(Packet* packet, Server& server);
void sendRegistryData(Packet& packet, Server& server);
void sendUpdateTags(Packet& packet, Server& server);

#endif
