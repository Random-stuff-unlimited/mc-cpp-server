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
void writeSpawnInfo(Buffer& buf, Player& player, Server& server, Level& level);
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
// ServerPlayerGameMode.tick: a block finished digging a little early breaks once its time is up
void tickDelayedDestroy(Server& server, Player& player);
void handleUseItemOnPacket(Packet& packet, Server& server);
void handleUseItemPacket(Packet& packet, Server& server);
void handleContainerClickPacket(Packet& packet, Server& server);
void handleContainerClosePacket(Packet& packet, Server& server);
void handleContainerButtonClickPacket(Packet& packet, Server& server);
void handleContainerSlotStateChangedPacket(Packet& packet, Server& server);
// Container clicks and recipe book (playPackets.cpp)
void sendInitialRecipeBook(Packet& packet, Server& server);
void handlePlaceRecipePacket(Packet& packet, Server& server);
void handleRecipeBookChangeSettingsPacket(Packet& packet, Server& server);
void handleSetCarriedItemPacket(Packet& packet, Server& server);
void handleSetCreativeModeSlotPacket(Packet& packet, Server& server);
void handlePickItemFromBlock(Packet& packet, Server& server);
void handlePlayPingRequest(Packet& packet, Server& server);
void sendServerResourcePack(Packet& packet, Server& server);
void handleResourcePackResponse(Packet& packet, Server& server);
void handleChangeDifficulty(Packet& packet, Server& server);
void handleLockDifficulty(Packet& packet, Server& server);
void handleChangeGameMode(Packet& packet, Server& server);
void handleBlockEntityTagQuery(Packet& packet, Server& server);
void handleEntityTagQuery(Packet& packet, Server& server);
void handlePickItemFromEntity(Packet& packet, Server& server);
void handleTeleportToEntity(Packet& packet, Server& server);
void handlePlayCustomPayload(Packet& packet, Server& server);
void tryPickItem(Packet& packet, Server& server, Player& player, Level& level, ItemStack picked, bool infinite);

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
// Shared by the state routers: the packet's player is disconnected (state None, PACKET_DISCONNECT)
void disconnect(Packet* packet);
void sendRegistryData(Packet& packet, Server& server);
void sendUpdateTags(Packet& packet, Server& server);

#endif
