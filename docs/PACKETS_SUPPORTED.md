# Paquets gérés

Minecraft 1.21.10. Généré par `tools/packet_status.py` (`make packets`), ne pas modifier à la main.

**80 paquets gérés sur 251** (171 manquants).

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
| `LOGIN_DISCONNECT` | 0x00 | `src/networking/packet/serverbound/handshake/handleHandshakePacket.cpp`, `src/server.cpp` |
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
| `DISCONNECT` | 0x02 | `src/networking/networkPacketRouter.cpp`, `src/server.cpp` |
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

### Serveur → client (37/139)

| Paquet | ID | Utilisé dans |
|---|---|---|
| `ADD_ENTITY` | 0x01 | `src/world/PlayerTracker.cpp` |
| `ANIMATE` | 0x02 | `src/networking/networkPacketRouter.cpp`, `src/world/Combat.cpp` |
| `BLOCK_CHANGED_ACK` | 0x04 | `src/networking/packet/serverbound/play/blockInteractionPackets.cpp` |
| `BLOCK_UPDATE` | 0x08 | `src/networking/packet/serverbound/play/blockInteractionPackets.cpp` |
| `CHANGE_DIFFICULTY` | 0x0A | `src/networking/packet/clientbound/play/changeDifficultyPacket.cpp` |
| `CHUNK_BATCH_FINISHED` | 0x0B | `src/world/ChunkStreamer.cpp` |
| `CHUNK_BATCH_START` | 0x0C | `src/world/ChunkStreamer.cpp` |
| `DAMAGE_EVENT` | 0x19 | `src/world/Combat.cpp` |
| `DISCONNECT` | 0x20 | `src/server.cpp` |
| `ENTITY_EVENT` | 0x22 | `src/world/Combat.cpp` |
| `ENTITY_POSITION_SYNC` | 0x23 | `src/world/PlayerTracker.cpp` |
| `FORGET_LEVEL_CHUNK` | 0x25 | `src/world/ChunkStreamer.cpp` |
| `GAME_EVENT` | 0x26 | `src/networking/packet/clientbound/play/gameEventPacket.cpp`, `src/world/Combat.cpp` |
| `HURT_ANIMATION` | 0x29 | `src/world/Combat.cpp` |
| `KEEP_ALIVE` | 0x2B | `src/server.cpp` |
| `LEVEL_CHUNK_WITH_LIGHT` | 0x2C | `src/world/World.cpp` |
| `LEVEL_EVENT` | 0x2D | `src/networking/packet/serverbound/play/blockInteractionPackets.cpp` |
| `LOGIN` | 0x30 | `src/networking/packet/clientbound/play/playPacket.cpp` |
| `MOVE_ENTITY_POS` | 0x33 | `src/world/PlayerTracker.cpp` |
| `MOVE_ENTITY_POS_ROT` | 0x34 | `src/world/PlayerTracker.cpp` |
| `MOVE_ENTITY_ROT` | 0x36 | `src/world/PlayerTracker.cpp` |
| `PLAYER_ABILITIES` | 0x3E | `src/networking/packet/clientbound/play/playerAbilitiesPacket.cpp` |
| `PLAYER_COMBAT_END` | 0x40 | `src/world/Combat.cpp` |
| `PLAYER_COMBAT_ENTER` | 0x41 | `src/world/Combat.cpp` |
| `PLAYER_COMBAT_KILL` | 0x42 | `src/world/Combat.cpp` |
| `PLAYER_INFO_REMOVE` | 0x43 | `src/world/PlayerTracker.cpp` |
| `PLAYER_INFO_UPDATE` | 0x44 | `src/world/PlayerTracker.cpp` |
| `PLAYER_POSITION` | 0x46 | `src/networking/packet/clientbound/play/synchronizePlayerPositionPacket.cpp` |
| `REMOVE_ENTITIES` | 0x4B | `src/world/PlayerTracker.cpp` |
| `RESPAWN` | 0x50 | `src/world/Combat.cpp` |
| `ROTATE_HEAD` | 0x51 | `src/world/PlayerTracker.cpp` |
| `SET_CHUNK_CACHE_CENTER` | 0x5C | `src/world/ChunkStreamer.cpp` |
| `SET_ENTITY_DATA` | 0x61 | `src/world/Combat.cpp` |
| `SET_ENTITY_MOTION` | 0x63 | `src/world/Combat.cpp` |
| `SET_HEALTH` | 0x66 | `src/world/Combat.cpp` |
| `SET_HELD_SLOT` | 0x67 | `src/networking/packet/clientbound/play/setHeldItemPacket.cpp` |
| `SYSTEM_CHAT` | 0x77 | `src/world/PlayerTracker.cpp` |

### Client → serveur (16/66)

| Paquet | ID | Utilisé dans |
|---|---|---|
| `ACCEPT_TELEPORTATION` | 0x00 | `src/networking/networkPacketRouter.cpp` |
| `CHUNK_BATCH_RECEIVED` | 0x0A | `src/networking/networkPacketRouter.cpp` |
| `CLIENT_COMMAND` | 0x0B | `src/networking/networkPacketRouter.cpp` |
| `INTERACT` | 0x19 | `src/networking/networkPacketRouter.cpp` |
| `KEEP_ALIVE` | 0x1B | `src/networking/networkPacketRouter.cpp` |
| `MOVE_PLAYER_POS` | 0x1D | `src/networking/networkPacketRouter.cpp` |
| `MOVE_PLAYER_POS_ROT` | 0x1E | `src/networking/networkPacketRouter.cpp` |
| `MOVE_PLAYER_ROT` | 0x1F | `src/networking/networkPacketRouter.cpp` |
| `MOVE_PLAYER_STATUS_ONLY` | 0x20 | `src/networking/networkPacketRouter.cpp` |
| `PLAYER_ACTION` | 0x28 | `src/networking/networkPacketRouter.cpp` |
| `PLAYER_COMMAND` | 0x29 | `src/networking/networkPacketRouter.cpp` |
| `PLAYER_LOADED` | 0x2B | `src/networking/networkPacketRouter.cpp` |
| `SET_CARRIED_ITEM` | 0x34 | `src/networking/networkPacketRouter.cpp` |
| `SET_CREATIVE_MODE_SLOT` | 0x37 | `src/networking/networkPacketRouter.cpp` |
| `SWING` | 0x3C | `src/networking/networkPacketRouter.cpp` |
| `USE_ITEM_ON` | 0x3F | `src/networking/networkPacketRouter.cpp` |
