#include "lib/UUID.hpp"
#include "network/buffer.hpp"
#include "network/networking.hpp"
#include "network/packet.hpp"
#include "player.hpp"
#include "network/server.hpp"

#include <iostream>

void sendPlayerAbilities(Packet& packet, Server& server) {
    std::cout << "=== Sending Player Abilities ===\n";

    Buffer buf;

    // Flags (byte) - bit field for player abilities
    uint8_t flags = 0x00;
    // 0x01: Invulnerable
    // 0x02: Flying
    // 0x04: Allow Flying (creative mode)
    // 0x08: Creative Mode (instant break)
    buf.writeByte(flags);

    // Flying Speed (float) - 0.05 by default
    // Convert 0.05f to IEEE 754 bits: 0x3D4CCCCD
    buf.writeInt(0x3D4CCCCD);

    // Field of View Modifier (float) - 0.1 by default
    // Convert 0.1f to IEEE 754 bits: 0x3DCCCCCD
    buf.writeInt(0x3DCCCCCD);

    int packetId = 0x39; // Player Abilities packet ID for protocol 770
    int packetIdSize = packet.getVarintSize(packetId);
    int totalPayloadSize = packetIdSize + buf.getData().size();

    Buffer finalBuf;
    finalBuf.writeVarInt(totalPayloadSize);
    finalBuf.writeVarInt(packetId);
    finalBuf.writeBytes(buf.getData());

    packet.getData() = finalBuf;
    packet.setPacketSize(finalBuf.getData().size());
    packet.setReturnPacket(PACKET_SEND);

    (void)server;
}

void sendSetHealth(Packet& packet, Server& server) {
    std::cout << "=== Sending Set Health ===\n";

    Buffer buf;

    // Health (float) - 20.0 = full health
    // Convert 20.0f to IEEE 754 bits: 0x41A00000
    buf.writeInt(0x41A00000);

    // Food (VarInt) - 20 = full food bar
    buf.writeVarInt(20);

    // Food Saturation (float) - 5.0 by default
    // Convert 5.0f to IEEE 754 bits: 0x40A00000
    buf.writeInt(0x40A00000);

    int packetId = 0x61; // Set Health packet ID for protocol 770
    int packetIdSize = packet.getVarintSize(packetId);
    int totalPayloadSize = packetIdSize + buf.getData().size();

    Buffer finalBuf;
    finalBuf.writeVarInt(totalPayloadSize);
    finalBuf.writeVarInt(packetId);
    finalBuf.writeBytes(buf.getData());

    packet.getData() = finalBuf;
    packet.setPacketSize(finalBuf.getData().size());
    packet.setReturnPacket(PACKET_SEND);

    (void)server;
}

void sendSetExperience(Packet& packet, Server& server) {
    std::cout << "=== Sending Set Experience ===\n";

    Buffer buf;

    // Experience bar (float) - 0.0 to 1.0 (progress to next level)
    // Convert 0.0f to IEEE 754 bits: 0x00000000
    buf.writeInt(0x00000000);

    // Level (VarInt) - current experience level
    buf.writeVarInt(0);

    // Total Experience (VarInt) - total experience points
    buf.writeVarInt(0);

    int packetId = 0x60; // Set Experience packet ID for protocol 770
    int packetIdSize = packet.getVarintSize(packetId);
    int totalPayloadSize = packetIdSize + buf.getData().size();

    Buffer finalBuf;
    finalBuf.writeVarInt(totalPayloadSize);
    finalBuf.writeVarInt(packetId);
    finalBuf.writeBytes(buf.getData());

    packet.getData() = finalBuf;
    packet.setPacketSize(finalBuf.getData().size());
    packet.setReturnPacket(PACKET_SEND);

    (void)server;
}

void sendUpdateTime(Packet& packet, Server& server) {
    std::cout << "=== Sending Update Time ===\n";

    Buffer buf;

    // World Age (Long) - total ticks since world creation
    buf.writeLong(0);

    // Time of day (Long) - 0 = sunrise, 6000 = noon, 12000 = sunset, 18000 = midnight
    buf.writeLong(1000); // Morning time

    // Time of day increasing (Boolean) - should client auto-advance time
    buf.writeByte(0x01); // true

    int packetId = 0x6A; // Update Time packet ID for protocol 770
    int packetIdSize = packet.getVarintSize(packetId);
    int totalPayloadSize = packetIdSize + buf.getData().size();

    Buffer finalBuf;
    finalBuf.writeVarInt(totalPayloadSize);
    finalBuf.writeVarInt(packetId);
    finalBuf.writeBytes(buf.getData());

    packet.getData() = finalBuf;
    packet.setPacketSize(finalBuf.getData().size());
    packet.setReturnPacket(PACKET_SEND);

    (void)server;
}

void sendSetHeldItem(Packet& packet, Server& server) {
    std::cout << "=== Sending Set Held Item ===\n";

    Buffer buf;

    // Slot (VarInt) - hotbar slot selected (0-8)
    buf.writeVarInt(0); // First slot selected

    int packetId = 0x62; // Set Held Item packet ID for protocol 770
    int packetIdSize = packet.getVarintSize(packetId);
    int totalPayloadSize = packetIdSize + buf.getData().size();

    Buffer finalBuf;
    finalBuf.writeVarInt(totalPayloadSize);
    finalBuf.writeVarInt(packetId);
    finalBuf.writeBytes(buf.getData());

    packet.getData() = finalBuf;
    packet.setPacketSize(finalBuf.getData().size());
    packet.setReturnPacket(PACKET_SEND);

    (void)server;
}

void handleConfirmTeleportation(Packet& packet, Server& server) {
    std::cout << "=== Received Confirm Teleportation ===\n";

    // Read teleport ID from packet data
    int teleportId = packet.getData().readVarInt();

    std::cout << "Player confirmed teleportation with ID: " << teleportId << std::endl;

    // Mark packet as processed
    packet.setReturnPacket(PACKET_OK);

    (void)server;
}

void completeSpawnSequence(Packet& packet, Server& server) {
    Player* player = packet.getPlayer();
    ThreadSafeQueue<Packet*>* outgoingPackets = server.getNetworkManager().getOutgoingQueue();
    if (!player || !outgoingPackets) return;

    // std::cout << "=== Completing spawn sequence for player: " << player->getPlayerName() << " ===\n";
    std::cout << "=== Completing spawn sequence for player: " << " ===\n";

    try {
        // 9. Player Abilities (0x39)
        Packet* abilitiesPacket = new Packet(packet);
        sendPlayerAbilities(*abilitiesPacket, server);
        outgoingPackets->push(abilitiesPacket);

        // 10. Set Health (0x61)
        Packet* healthPacket = new Packet(packet);
        sendSetHealth(*healthPacket, server);
        outgoingPackets->push(healthPacket);

        // 11. Set Experience (0x60)
        Packet* experiencePacket = new Packet(packet);
        sendSetExperience(*experiencePacket, server);
        outgoingPackets->push(experiencePacket);

        // 12. Update Time (0x6A)
        Packet* timePacket = new Packet(packet);
        sendUpdateTime(*timePacket, server);
        outgoingPackets->push(timePacket);

        // 13. Set Held Item (0x62)
        Packet* heldItemPacket = new Packet(packet);
        sendSetHeldItem(*heldItemPacket, server);
        outgoingPackets->push(heldItemPacket);

        std::cout << "=== Spawn sequence completed! Player should now be fully spawned ===\n";

    } catch (const std::exception& e) {
        std::cerr << "Error completing spawn sequence: " << e.what() << std::endl;
    }
}
