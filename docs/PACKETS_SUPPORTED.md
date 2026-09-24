# Paquets gérés

Minecraft 1.21.10. Généré par `tools/packet_status.py` (`make packets`), ne pas modifier à la main.

**53 paquets gérés sur 251** (198 manquants).

## Handshake

### Client → serveur (1/1)

| Paquet | ID | Utilisé dans |
|---|---|---|
| `INTENTION` | 0x00 | `src/networking/networkPacketRouter.cpp` |

## Status

### Serveur → client (2/2)

| Paquet | ID | Utilisé dans |
|---|---|---|
| `STATUS_RESPONSE` | 0x00 | `src/networking/packet/serverbound/status/handleStatusPacket.cpp` |
| `PONG_RESPONSE` | 0x01 | `src/networking/packet/serverbound/status/handlePingPacket.cpp` |

### Client → serveur (2/2)

| Paquet | ID | Utilisé dans |
|---|---|---|
| `STATUS_REQUEST` | 0x00 | `src/networking/networkPacketRouter.cpp` |
| `PING_REQUEST` | 0x01 | `src/networking/networkPacketRouter.cpp` |

## Login

### Serveur → client (3/6)

| Paquet | ID | Utilisé dans |
|---|---|---|
| `LOGIN_DISCONNECT` | 0x00 | `src/networking/packet/serverbound/handshake/handleHandshakePacket.cpp` |
| `LOGIN_FINISHED` | 0x02 | `src/networking/packet/serverbound/login/handleLoginStartPacket.cpp` |
| `LOGIN_COMPRESSION` | 0x03 | `src/networking/packet/serverbound/login/handleLoginStartPacket.cpp` |

### Client → serveur (4/5)

| Paquet | ID | Utilisé dans |
|---|---|---|
| `HELLO` | 0x00 | `src/networking/networkPacketRouter.cpp` |
| `CUSTOM_QUERY_ANSWER` | 0x02 | `src/networking/networkPacketRouter.cpp` |
| `LOGIN_ACKNOWLEDGED` | 0x03 | `src/networking/networkPacketRouter.cpp` |
| `COOKIE_RESPONSE` | 0x04 | `src/networking/networkPacketRouter.cpp` |

## Configuration

### Serveur → client (6/20)

| Paquet | ID | Utilisé dans |
|---|---|---|
| `DISCONNECT` | 0x02 | `src/networking/networkPacketRouter.cpp` |
| `FINISH_CONFIGURATION` | 0x03 | `src/networking/packet/clientbound/configuration/handleFinishConfigurationPacket.cpp` |
| `REGISTRY_DATA` | 0x07 | `src/networking/packet/clientbound/configuration/registryDataPacket.cpp` |
| `UPDATE_ENABLED_FEATURES` | 0x0C | `src/networking/packet/clientbound/configuration/clientboundFeatureFlagsPacket.cpp` |
| `UPDATE_TAGS` | 0x0D | `src/networking/packet/clientbound/configuration/updateTagsPacket.cpp` |
| `SELECT_KNOWN_PACKS` | 0x0E | `src/networking/packet/clientbound/configuration/clientboundKnownPacksPacket.cpp` |

### Client → serveur (9/10)

| Paquet | ID | Utilisé dans |
|---|---|---|
| `CLIENT_INFORMATION` | 0x00 | `src/networking/networkPacketRouter.cpp` |
| `COOKIE_RESPONSE` | 0x01 | `src/networking/networkPacketRouter.cpp` |
| `CUSTOM_PAYLOAD` | 0x02 | `src/networking/networkPacketRouter.cpp` |
| `FINISH_CONFIGURATION` | 0x03 | `src/networking/networkPacketRouter.cpp` |
| `KEEP_ALIVE` | 0x04 | `src/networking/networkPacketRouter.cpp` |
| `PONG` | 0x05 | `src/networking/networkPacketRouter.cpp` |
| `RESOURCE_PACK` | 0x06 | `src/networking/networkPacketRouter.cpp` |
| `SELECT_KNOWN_PACKS` | 0x07 | `src/networking/networkPacketRouter.cpp` |
| `CUSTOM_CLICK_ACTION` | 0x08 | `src/networking/networkPacketRouter.cpp` |

## Play

### Serveur → client (15/139)

| Paquet | ID | Utilisé dans |
|---|---|---|
| `BLOCK_CHANGED_ACK` | 0x04 | `src/networking/packet/serverbound/play/blockInteractionPackets.cpp` |
| `BLOCK_UPDATE` | 0x08 | `src/networking/packet/serverbound/play/blockInteractionPackets.cpp` |
| `CHANGE_DIFFICULTY` | 0x0A | `src/networking/packet/clientbound/play/changeDifficultyPacket.cpp` |
| `CHUNK_BATCH_FINISHED` | 0x0B | `src/world/ChunkStreamer.cpp` |
| `CHUNK_BATCH_START` | 0x0C | `src/world/ChunkStreamer.cpp` |
| `FORGET_LEVEL_CHUNK` | 0x25 | `src/world/ChunkStreamer.cpp` |
| `GAME_EVENT` | 0x26 | `src/networking/packet/clientbound/play/gameEventPacket.cpp` |
| `KEEP_ALIVE` | 0x2B | `src/server.cpp` |
| `LEVEL_CHUNK_WITH_LIGHT` | 0x2C | `src/world/World.cpp` |
| `LEVEL_EVENT` | 0x2D | `src/networking/packet/serverbound/play/blockInteractionPackets.cpp` |
| `LOGIN` | 0x30 | `src/networking/packet/clientbound/play/playPacket.cpp` |
| `PLAYER_ABILITIES` | 0x3E | `src/networking/packet/clientbound/play/playerAbilitiesPacket.cpp` |
| `PLAYER_POSITION` | 0x46 | `src/networking/packet/clientbound/play/synchronizePlayerPositionPacket.cpp` |
| `SET_CHUNK_CACHE_CENTER` | 0x5C | `src/world/ChunkStreamer.cpp` |
| `SET_HELD_SLOT` | 0x67 | `src/networking/packet/clientbound/play/setHeldItemPacket.cpp` |

### Client → serveur (11/66)

| Paquet | ID | Utilisé dans |
|---|---|---|
| `ACCEPT_TELEPORTATION` | 0x00 | `src/networking/networkPacketRouter.cpp` |
| `CHUNK_BATCH_RECEIVED` | 0x0A | `src/networking/networkPacketRouter.cpp` |
| `KEEP_ALIVE` | 0x1B | `src/networking/networkPacketRouter.cpp` |
| `MOVE_PLAYER_POS` | 0x1D | `src/networking/networkPacketRouter.cpp` |
| `MOVE_PLAYER_POS_ROT` | 0x1E | `src/networking/networkPacketRouter.cpp` |
| `MOVE_PLAYER_ROT` | 0x1F | `src/networking/networkPacketRouter.cpp` |
| `PLAYER_ACTION` | 0x28 | `src/networking/networkPacketRouter.cpp` |
| `PLAYER_LOADED` | 0x2B | `src/networking/networkPacketRouter.cpp` |
| `SET_CARRIED_ITEM` | 0x34 | `src/networking/networkPacketRouter.cpp` |
| `SET_CREATIVE_MODE_SLOT` | 0x37 | `src/networking/networkPacketRouter.cpp` |
| `USE_ITEM_ON` | 0x3F | `src/networking/networkPacketRouter.cpp` |
