#ifndef PACKET_ROUTER_HPP
# define PACKET_ROUTER_HPP

# include "packet.hpp"
# include "server.hpp"

// clientbound
void changeDifficultyPacket(Packet& packet, Server& server);
void clientboundKnownPacksPacket(Packet& packet, Server& server);
void gameEventPacket(Packet& packet, Server& server);
void handleCookieRequestPacket(Packet& packet, Server& server);
void handleFinishConfigurationPacket(Packet& packet, Server& server);
void handleLoginStartPacket(Packet& packet, Server& server);
void handlePingPacket(Packet& packet, Server& server);
void handleStatusPacket(Packet& packet, Server& server);
void levelChunkWithLightPacket(Packet& packet, Server& server);
void playerAbilitiesPacket(Packet& packet, Server& server);
void sendPlayPacket(Packet& packet, Server& server);
void setHeldItemPacket(Packet& packet, Server& server);
void synchronizePlayerPositionPacket(Packet& packet, Server& server);
void setCenterPacket(Packet& packet, Server& server);

// serverbound
void handleAcknowledgeFinishConfigurationPacket(Packet& packet, Server& server);
void handleClientInformationPacket(Packet& packet, Server& server);
void handleConfirmTeleportationPacket(Packet& packet, Server& server);
void handleHandshakePacket(Packet& packet, Server& server);
void handleLoginAcknowledgedPacket(Packet& packet, Server& server);
void serverboundKnownPacksPacket(Packet& packet);

void packetRouter(Packet* packet, Server& server);

void handleHandshakeState(Packet* packet, Server& server);
void handleStatusState(Packet* packet, Server& server);
void handleLoginState(Packet* packet, Server& server);
void handleConfigurationState(Packet* packet, Server& server);
void handlePlayState(Packet* packet, Server& server);
void sendRegistryData(Packet& packet, Server& server);
void sendUpdateTags(Packet& packet, Server& server);

#endif
