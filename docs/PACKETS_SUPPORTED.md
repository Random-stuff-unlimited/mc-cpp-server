# Paquets gérés

Minecraft 1.21.10. Généré par `tools/packet_status.py` (`make packets`), ne pas modifier à la main.

**117 paquets gérés sur 251** (134 manquants).

## Handshake

### Client → serveur (1/1)

| Paquet | ID | Utilisé dans |
|---|---|---|
| `INTENTION` | 0x00 | `src/networking/packet/router/handshakeState.cpp` |

## Status

### Serveur → client (2/2)

| Paquet | ID | Utilisé dans |
|---|---|---|
| `STATUS_RESPONSE` | 0x00 | `src/networking/packet/serverbound/status/statusPackets.cpp` |
| `PONG_RESPONSE` | 0x01 | `src/networking/packet/serverbound/status/statusPackets.cpp` |

### Client → serveur (2/2)

| Paquet | ID | Utilisé dans |
|---|---|---|
| `STATUS_REQUEST` | 0x00 | `src/networking/packet/router/statusState.cpp`, `src/networking/packet/serverbound/status/statusPackets.cpp` |
| `PING_REQUEST` | 0x01 | `src/networking/packet/router/statusState.cpp`, `src/networking/packet/serverbound/status/statusPackets.cpp` |

## Login

### Serveur → client (3/6)

| Paquet | ID | Utilisé dans |
|---|---|---|
| `LOGIN_DISCONNECT` | 0x00 | `src/networking/packet/serverbound/handshake/handleHandshakePacket.cpp`, `src/server.cpp` |
| `LOGIN_FINISHED` | 0x02 | `src/networking/packet/serverbound/login/loginPackets.cpp` |
| `LOGIN_COMPRESSION` | 0x03 | `src/networking/packet/serverbound/login/loginPackets.cpp` |

### Client → serveur (4/5)

| Paquet | ID | Utilisé dans |
|---|---|---|
| `HELLO` | 0x00 | `src/networking/packet/router/loginState.cpp` |
| `CUSTOM_QUERY_ANSWER` | 0x02 | `src/networking/packet/router/loginState.cpp` |
| `LOGIN_ACKNOWLEDGED` | 0x03 | `src/networking/packet/router/loginState.cpp` |
| `COOKIE_RESPONSE` | 0x04 | `src/networking/packet/router/loginState.cpp` |

## Configuration

### Serveur → client (6/20)

| Paquet | ID | Utilisé dans |
|---|---|---|
| `DISCONNECT` | 0x02 | `src/networking/packet/router/configurationState.cpp`, `src/server.cpp` |
| `FINISH_CONFIGURATION` | 0x03 | `src/networking/packet/clientbound/configuration/configurationPackets.cpp` |
| `REGISTRY_DATA` | 0x07 | `src/networking/packet/clientbound/configuration/configurationPackets.cpp` |
| `UPDATE_ENABLED_FEATURES` | 0x0C | `src/networking/packet/clientbound/configuration/configurationPackets.cpp` |
| `UPDATE_TAGS` | 0x0D | `src/networking/packet/clientbound/configuration/configurationPackets.cpp` |
| `SELECT_KNOWN_PACKS` | 0x0E | `src/networking/packet/clientbound/configuration/configurationPackets.cpp` |

### Client → serveur (9/10)

| Paquet | ID | Utilisé dans |
|---|---|---|
| `CLIENT_INFORMATION` | 0x00 | `src/networking/packet/router/configurationState.cpp` |
| `COOKIE_RESPONSE` | 0x01 | `src/networking/packet/router/configurationState.cpp` |
| `CUSTOM_PAYLOAD` | 0x02 | `src/networking/packet/router/configurationState.cpp` |
| `FINISH_CONFIGURATION` | 0x03 | `src/networking/networkWorker.cpp` |
| `KEEP_ALIVE` | 0x04 | `src/networking/packet/router/configurationState.cpp` |
| `PONG` | 0x05 | `src/networking/packet/router/configurationState.cpp` |
| `RESOURCE_PACK` | 0x06 | `src/networking/packet/router/configurationState.cpp` |
| `SELECT_KNOWN_PACKS` | 0x07 | `src/networking/packet/router/configurationState.cpp` |
| `CUSTOM_CLICK_ACTION` | 0x08 | `src/networking/packet/router/configurationState.cpp` |

## Play

### Serveur → client (57/139)

| Paquet | ID | Utilisé dans |
|---|---|---|
| `ADD_ENTITY` | 0x01 | `src/world/PlayerTracker.cpp`, `src/world/entity/EntityManager.cpp` |
| `ANIMATE` | 0x02 | `src/networking/packet/router/playState.cpp`, `src/server.cpp`, `src/world/Combat.cpp` |
| `BLOCK_CHANGED_ACK` | 0x04 | `src/server.cpp` |
| `BLOCK_ENTITY_DATA` | 0x06 | `src/world/Level.cpp` |
| `BLOCK_EVENT` | 0x07 | `src/world/Level.cpp` |
| `BLOCK_UPDATE` | 0x08 | `src/networking/packet/serverbound/play/blockInteractionPackets.cpp`, `src/world/Level.cpp` |
| `CHANGE_DIFFICULTY` | 0x0A | `src/networking/packet/clientbound/play/playPackets.cpp` |
| `CHUNK_BATCH_FINISHED` | 0x0B | `src/world/ChunkStreamer.cpp` |
| `CHUNK_BATCH_START` | 0x0C | `src/world/ChunkStreamer.cpp` |
| `CONTAINER_CLOSE` | 0x11 | `src/world/inventory/Menu.cpp` |
| `CONTAINER_SET_CONTENT` | 0x12 | `src/world/inventory/Menu.cpp` |
| `CONTAINER_SET_DATA` | 0x13 | `src/world/inventory/Menu.cpp` |
| `CONTAINER_SET_SLOT` | 0x14 | `src/world/inventory/Menu.cpp` |
| `DAMAGE_EVENT` | 0x19 | `src/world/Combat.cpp`, `src/world/entity/LivingEntity.cpp` |
| `DISCONNECT` | 0x20 | `src/server.cpp` |
| `ENTITY_EVENT` | 0x22 | `src/world/Combat.cpp`, `src/world/Survival.cpp`, `src/world/entity/LivingEntity.cpp`, `src/world/item/ItemUse.cpp` |
| `ENTITY_POSITION_SYNC` | 0x23 | `src/world/PlayerTracker.cpp`, `src/world/entity/EntityManager.cpp` |
| `FORGET_LEVEL_CHUNK` | 0x25 | `src/world/ChunkStreamer.cpp` |
| `GAME_EVENT` | 0x26 | `src/networking/packet/clientbound/play/playPackets.cpp`, `src/server.cpp`, `src/world/Combat.cpp` |
| `HURT_ANIMATION` | 0x29 | `src/world/Combat.cpp` |
| `KEEP_ALIVE` | 0x2B | `src/server.cpp` |
| `LEVEL_CHUNK_WITH_LIGHT` | 0x2C | `src/world/World.cpp` |
| `LEVEL_EVENT` | 0x2D | `src/world/Level.cpp` |
| `LEVEL_PARTICLES` | 0x2E | `src/world/blockentity/StorageEntities.cpp` |
| `LIGHT_UPDATE` | 0x2F | `src/world/Level.cpp` |
| `LOGIN` | 0x30 | `src/networking/packet/clientbound/play/playPackets.cpp` |
| `MOVE_ENTITY_POS` | 0x33 | `src/world/PlayerTracker.cpp`, `src/world/entity/EntityManager.cpp` |
| `MOVE_ENTITY_POS_ROT` | 0x34 | `src/world/PlayerTracker.cpp`, `src/world/entity/EntityManager.cpp` |
| `MOVE_ENTITY_ROT` | 0x36 | `src/world/PlayerTracker.cpp`, `src/world/entity/EntityManager.cpp` |
| `OPEN_SCREEN` | 0x39 | `src/world/inventory/Menu.cpp` |
| `PLACE_GHOST_RECIPE` | 0x3D | `src/networking/packet/serverbound/play/playPackets.cpp` |
| `PLAYER_ABILITIES` | 0x3E | `src/networking/packet/clientbound/play/playPackets.cpp` |
| `PLAYER_COMBAT_END` | 0x40 | `src/world/Combat.cpp` |
| `PLAYER_COMBAT_ENTER` | 0x41 | `src/world/Combat.cpp` |
| `PLAYER_COMBAT_KILL` | 0x42 | `src/world/Combat.cpp` |
| `PLAYER_INFO_REMOVE` | 0x43 | `src/world/PlayerTracker.cpp` |
| `PLAYER_INFO_UPDATE` | 0x44 | `src/world/PlayerTracker.cpp` |
| `PLAYER_POSITION` | 0x46 | `src/networking/packet/clientbound/play/playPackets.cpp`, `src/world/blocks/Attached.cpp` |
| `RECIPE_BOOK_ADD` | 0x48 | `src/networking/packet/serverbound/play/playPackets.cpp` |
| `RECIPE_BOOK_SETTINGS` | 0x4A | `src/networking/packet/serverbound/play/playPackets.cpp` |
| `REMOVE_ENTITIES` | 0x4B | `src/world/PlayerTracker.cpp`, `src/world/entity/EntityManager.cpp` |
| `RESPAWN` | 0x50 | `src/world/Combat.cpp` |
| `ROTATE_HEAD` | 0x51 | `src/world/PlayerTracker.cpp`, `src/world/entity/EntityManager.cpp` |
| `SECTION_BLOCKS_UPDATE` | 0x52 | `src/world/Level.cpp` |
| `SET_CHUNK_CACHE_CENTER` | 0x5C | `src/world/ChunkStreamer.cpp` |
| `SET_CURSOR_ITEM` | 0x5E | `src/world/inventory/Menu.cpp` |
| `SET_ENTITY_DATA` | 0x61 | `src/world/Combat.cpp`, `src/world/PlayerTracker.cpp`, `src/world/Survival.cpp`, `src/world/entity/EntityManager.cpp` |
| `SET_ENTITY_MOTION` | 0x63 | `src/world/Combat.cpp`, `src/world/entity/EntityManager.cpp` |
| `SET_HEALTH` | 0x66 | `src/world/Survival.cpp` |
| `SET_HELD_SLOT` | 0x67 | `src/networking/packet/clientbound/play/playPackets.cpp` |
| `SET_TIME` | 0x6F | `src/server.cpp` |
| `SOUND` | 0x73 | `src/world/Level.cpp` |
| `SYSTEM_CHAT` | 0x77 | `src/Commands.cpp`, `src/server.cpp`, `src/world/Combat.cpp`, `src/world/PlayerTracker.cpp` |
| `TAKE_ITEM_ENTITY` | 0x7A | `src/world/entity/EntityManager.cpp` |
| `TICKING_STATE` | 0x7D | `src/server.cpp` |
| `TICKING_STEP` | 0x7E | `src/server.cpp` |
| `UPDATE_ATTRIBUTES` | 0x81 | `src/world/entity/EntityManager.cpp` |

### Client → serveur (33/66)

| Paquet | ID | Utilisé dans |
|---|---|---|
| `ACCEPT_TELEPORTATION` | 0x00 | `src/networking/packet/router/playState.cpp` |
| `CHAT_ACK` | 0x05 | `src/networking/packet/router/playState.cpp` |
| `CHAT_COMMAND` | 0x06 | `src/networking/packet/router/playState.cpp` |
| `CHAT_COMMAND_SIGNED` | 0x07 | `src/networking/packet/router/playState.cpp` |
| `CHAT` | 0x08 | `src/networking/packet/router/playState.cpp` |
| `CHAT_SESSION_UPDATE` | 0x09 | `src/networking/packet/router/playState.cpp` |
| `CHUNK_BATCH_RECEIVED` | 0x0A | `src/networking/packet/router/playState.cpp` |
| `CLIENT_COMMAND` | 0x0B | `src/networking/packet/router/playState.cpp` |
| `CLIENT_INFORMATION` | 0x0D | `src/networking/packet/router/playState.cpp` |
| `CONTAINER_BUTTON_CLICK` | 0x10 | `src/networking/packet/router/playState.cpp` |
| `CONTAINER_CLICK` | 0x11 | `src/networking/packet/router/playState.cpp` |
| `CONTAINER_CLOSE` | 0x12 | `src/networking/packet/router/playState.cpp` |
| `CONTAINER_SLOT_STATE_CHANGED` | 0x13 | `src/networking/packet/router/playState.cpp` |
| `INTERACT` | 0x19 | `src/networking/packet/router/playState.cpp` |
| `KEEP_ALIVE` | 0x1B | `src/networking/packet/router/playState.cpp` |
| `MOVE_PLAYER_POS` | 0x1D | `src/networking/packet/router/playState.cpp` |
| `MOVE_PLAYER_POS_ROT` | 0x1E | `src/networking/packet/router/playState.cpp` |
| `MOVE_PLAYER_ROT` | 0x1F | `src/networking/packet/router/playState.cpp` |
| `MOVE_PLAYER_STATUS_ONLY` | 0x20 | `src/networking/packet/router/playState.cpp` |
| `PICK_ITEM_FROM_BLOCK` | 0x23 | `src/networking/packet/router/playState.cpp` |
| `PLACE_RECIPE` | 0x26 | `src/networking/packet/router/playState.cpp` |
| `PLAYER_ABILITIES` | 0x27 | `src/networking/packet/router/playState.cpp` |
| `PLAYER_ACTION` | 0x28 | `src/networking/packet/router/playState.cpp` |
| `PLAYER_COMMAND` | 0x29 | `src/networking/packet/router/playState.cpp` |
| `PLAYER_INPUT` | 0x2A | `src/networking/packet/router/playState.cpp` |
| `PLAYER_LOADED` | 0x2B | `src/networking/packet/router/playState.cpp` |
| `RECIPE_BOOK_CHANGE_SETTINGS` | 0x2D | `src/networking/packet/router/playState.cpp` |
| `RECIPE_BOOK_SEEN_RECIPE` | 0x2E | `src/networking/packet/router/playState.cpp` |
| `SET_CARRIED_ITEM` | 0x34 | `src/networking/packet/router/playState.cpp` |
| `SET_CREATIVE_MODE_SLOT` | 0x37 | `src/networking/packet/router/playState.cpp` |
| `SWING` | 0x3C | `src/networking/packet/router/playState.cpp` |
| `USE_ITEM_ON` | 0x3F | `src/networking/packet/router/playState.cpp` |
| `USE_ITEM` | 0x40 | `src/networking/packet/router/playState.cpp` |
