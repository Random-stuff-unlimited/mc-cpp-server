#include "UUID.hpp"
#include "buffer.hpp"
#include "networking.hpp"
#include "packet.hpp"
#include "player.hpp"
#include "server.hpp"

#include <iostream>

void sendChunkBatchStart(Packet& packet, Server& server) {
	std::cout << "=== Sending Chunk Batch Start ===\n";

	Buffer buf; // pas de données pour ce packet

	int packetId = 0x0C; // Chunk Batch Start packet ID

	Buffer payload;
	payload.writeVarInt(packetId);     // packet ID
	payload.writeBytes(buf.getData()); // aucune donnée ici

	Buffer finalBuf;
	finalBuf.writeVarInt(payload.getData().size()); // taille totale
	finalBuf.writeBytes(payload.getData());

	packet.getData() = finalBuf;
	packet.setPacketSize(finalBuf.getData().size());
	packet.setReturnPacket(PACKET_SEND);

	(void)server;
}

void sendChunkBatchFinished(Packet& packet, Server& server, int batchSize) {
	std::cout << "=== Sending Chunk Batch Finished (batch size: " << batchSize << ") ===\n";

	Buffer buf;
	buf.writeVarInt(batchSize); // nombre de chunks dans le batch

	int packetId = 0x0B; // Chunk Batch Finished packet ID

	Buffer payload;
	payload.writeVarInt(packetId); // packet ID
	payload.writeBytes(buf.getData());

	Buffer finalBuf;
	finalBuf.writeVarInt(payload.getData().size()); // taille totale
	finalBuf.writeBytes(payload.getData());

	packet.getData() = finalBuf;
	packet.setPacketSize(finalBuf.getData().size());
	packet.setReturnPacket(PACKET_SEND);

	(void)server;
}

void sendChunkBatchSequence(Packet& packet,
                            Server& server,
                            ThreadSafeQueue<Packet*>* outgoingPackets) {
	Player* player = packet.getPlayer();
	if (!player || !outgoingPackets) return;
	;

	// Get view distance from player config
	// int viewDistance = 5; // Default
	// if (player->getPlayerConfig()) {
	// 	viewDistance = player->getPlayerConfig()->getViewDistance();
	// }

	// std::cout << "=== Starting chunk batch sequence for player: " << player->getPlayerName()
	//           << " (view distance: " << viewDistance << ") ===\n";

	// 1. Send Chunk Batch Start
	try {
		Packet* batchStartPacket = new Packet(packet);
		sendChunkBatchStart(*batchStartPacket, server);
		outgoingPackets->push(batchStartPacket);
	} catch (const std::exception& e) {
		std::cerr << "Error sending chunk batch start: " << e.what() << std::endl;
		return;
	}

	static int i = 0;

	// Player spawn position (you should get this from player data)
	int playerChunkX = 0; // player->getChunkX();
	int playerChunkZ = 0; // player->getChunkZ()
	int viewDistance = 5;
	int chunksCount  = 0;
	int startX       = playerChunkX - viewDistance;
	int endX         = playerChunkX + viewDistance;
	int startZ       = playerChunkZ - viewDistance;
	int endZ         = playerChunkZ + viewDistance;

	for (int x = startX; x <= endX; x++) {
		for (int z = startZ; z <= endZ; z++) {
			try {
				i++;
				Packet* chunkPacket = new Packet(packet);
				sendChunkData(*chunkPacket, server, x, z);
				outgoingPackets->push(chunkPacket);
				chunksCount++;
			} catch (const std::exception& e) {
				std::cerr << "Error sending chunk (" << x << ", " << z << "): " << e.what()
				          << std::endl;
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
