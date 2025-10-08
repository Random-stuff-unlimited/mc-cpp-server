#include "lib/UUID.hpp"
#include "network/buffer.hpp"
#include "network/networking.hpp"
#include "network/packet.hpp"
#include "player.hpp"
#include "network/server.hpp"

#include <iostream>

void sendChunkBatchStart(Packet& packet, Server& server) {
    std::cout << "=== Sending Chunk Batch Start ===\n";

    // Chunk Batch Start has no fields - just the packet ID
    Buffer buf;
    // No data to write for this packet

    int packetId = 0x0C; // Chunk Batch Start packet ID for protocol 770
    int packetIdSize = packet.getVarintSize(packetId);
    int totalPayloadSize = packetIdSize + buf.getData().size();

    Buffer finalBuf;
    finalBuf.writeVarInt(totalPayloadSize);
    finalBuf.writeVarInt(packetId);
    if (!buf.getData().empty()) {
        finalBuf.writeBytes(buf.getData());
    }

    packet.getData() = finalBuf;
    packet.setPacketSize(finalBuf.getData().size());
    packet.setReturnPacket(PACKET_SEND);

    (void)server;
}

void sendChunkBatchFinished(Packet& packet, Server& server, int batchSize) {
    std::cout << "=== Sending Chunk Batch Finished (batch size: " << batchSize << ") ===\n";

    Buffer buf;
    buf.writeVarInt(batchSize); // Number of chunks in the batch

    int packetId = 0x0B; // Chunk Batch Finished packet ID for protocol 770
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

void sendChunkBatchSequence(Packet& packet, Server& server, ThreadSafeQueue<Packet*>* outgoingPackets) {
    Player* player = packet.getPlayer();
    if (!player || !outgoingPackets) return;

    // Player spawn position (you should get this from player data)
    int playerChunkX = 0; // player->getChunkX();
    int playerChunkZ = 0; // player->getChunkZ();

    // Get view distance from player config
    int viewDistance = 5; // Default
    if (player->getPlayerConfig()) {
        viewDistance = player->getPlayerConfig()->getViewDistance();
    }

    std::cout << "=== Starting chunk batch sequence for player: " << player->getPlayerName()
              << " (view distance: " << viewDistance << ") ===\n";

    // 1. Send Chunk Batch Start
    try {
        Packet* batchStartPacket = new Packet(packet);
        sendChunkBatchStart(*batchStartPacket, server);
        outgoingPackets->push(batchStartPacket);
    } catch (const std::exception& e) {
        std::cerr << "Error sending chunk batch start: " << e.what() << std::endl;
        return;
    }

    // 2. Send chunks in a radius around player
    int chunksCount = 0;
    for (int x = playerChunkX - viewDistance; x <= playerChunkX + viewDistance; x++) {
        for (int z = playerChunkZ - viewDistance; z <= playerChunkZ + viewDistance; z++) {
            try {
                Packet* chunkPacket = new Packet(packet);
                sendChunkData(*chunkPacket, server, x, z);
                outgoingPackets->push(chunkPacket);
                chunksCount++;
            } catch (const std::exception& e) {
                std::cerr << "Error sending chunk (" << x << ", " << z << "): " << e.what() << std::endl;
            }
        }
    }

    // 3. Send Chunk Batch Finished
    try {
        Packet* batchFinishedPacket = new Packet(packet);
        sendChunkBatchFinished(*batchFinishedPacket, server, chunksCount);
        outgoingPackets->push(batchFinishedPacket);
    } catch (const std::exception& e) {
        std::cerr << "Error sending chunk batch finished: " << e.what() << std::endl;
        return;
    }

    std::cout << "=== Chunk batch sequence completed: " << chunksCount << " chunks sent ===\n";
}
