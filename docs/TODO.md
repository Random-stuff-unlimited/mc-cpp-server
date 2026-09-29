# TODO — Ce qui reste à faire

Inventaire non exhaustif des gros blocs restants, à l'exclusion de la génération de monde (générateurs Flat/Void
conservés tels quels). À mettre à jour au fil de l'avancement.

## Progression (XP / enchantement)

- [x] Système d'XP : `SET_EXPERIENCE`, état (total / niveau / progression) sur le joueur, `world/Xp.cpp`
- [x] Orbes d'XP : entité `ExperienceOrb` (gravité, fusion, attraction vers le joueur, ramassage), valeur dans le packet Spawn Entity
- [x] Gagner de l'XP au minage (minerais), en tuant des mobs, à la cuisson (fours), et perte à la mort (orbs + réduction)
- [x] Persistance de l'XP dans playerdata (`XpTotal`, `XpLevel`, `XpP`, `XpSeed`)
- [x] Table d'enchantement : `EnchantmentMenu` (`minecraft:enchantment`), offre de 3 options (seed + étagères ≤ 15,
      coût requis = `calculateRequiredExperienceLevel`, ench. = tag `in_enchanting_table`), coût payé = 1-3 niveaux +
      1-3 lapis, livre → livre enchanté, `XpSeed` régénéré après l'enchantement, son du bloc
- [x] Enclume : `AnvilMenu` (`minecraft:anvil`), réparation au matériau (¼ de durabilité par item), combinaison de deux
      items du même type, combinaison d'enchantements (coût `anvil_cost`, livres halvés, incompatibilités, max level),
      renommage (`RENAME_ITEM`, `custom_name`, ≤ 50 chars, strip des caractères invalides), coût en XP, `repair_cost`,
      limite « Too Expensive » (39), usure/rupture de l'enclume (12 %), sons
- [x] Fioles enchantées (`minecraft:experience_bottle`) : entité `ThrownExperienceBottle` (gravité, collision, 600 ticks),
      casse → 3-11 XP en orbe + sons verre/ramassage ; 1 fiole utilisée
- [x] Sons : `entity.player.levelup` quand le niveau passe par un multiple de 5 (pitch niveau/30),
      `entity.experience_orb.pickup` au ramassage
- [x] Livres enchantés : les enchantements vont dans `minecraft:stored_enchantments` (lecture/écriture par item)

## Commandes

Implémentées : `tp`, `gamemode`/`gm`, `spawn`, `setworldspawn`, `spawnpoint`, `execute in … run tp`, `fill`, `help`,
`time`, `list`, `setblock`, `give`, `clear`, `kill`, `difficulty`, `weather`.

- [ ] Admin : `/kick`, `/ban`, `/op`, `/deop`, `/say`, `/tell` + op/permissions (whitelist, op list, gate des commandes)
- [ ] `/summon <mob>` (via `MobRegistry::spawn`)
- [ ] `/effect <joueur> <effet> [durée] [niveau]` (une fois les effets de statut faits)
- [ ] `/xp <joueur> <niveau|points>` (donner/retirer de l'XP)
- [ ] `/enchant <joueur> <ench> [niveau]`
- [ ] `/weather <durée>`, `/time set <durée>` (durée en secondes) : persistance + durées explicites
- [ ] Autocomplétion des commandes côté client (`COMMANDS` + `COMMAND_SUGGESTIONS`, `Commands::suggest` existe déjà)

## Effets de statut / potions

- [ ] Moteur d'effets de statut (vitesse, force, régénération, poison…), durée et niveaux
- [ ] Envoyer les effets au client : `UPDATE_MOB_EFFECT`, `REMOVE_MOB_EFFECT`, `ENTITY_EVENT` (guérison)
- [ ] Application à l'entité : `LivingEntity.addEffect`, tick des effets, dégâts périodiques (poison, warding…)
- [ ] Effets des potions bues (`ItemUse` → effet), potions jetables, flèches à effets
- [ ] Défenses/mitigations liées aux effets (résistance, absorption, résistance au feu)
- [ ] Beacon : `SET_BEACON`, effets du beacon, coût en items

## Mobs & IA

Solide : goals (19), pathfinding, équipement (`SET_EQUIPMENT`), élevage (amour/bébés/nourriture), spawn naturel,
despawn, conversion (`convertTo`), combat à distance (flèches), vol (bat), sauts (slime/rabbit), mobs spécifiques :
zombie/husk/drowned, creeper, spider/cave spider, enderman, slime/magma cube, cow/pig/chicken/sheep/rabbit,
skeleton/stray, wolf (apprivoisable), zombified piglin, polar bear, bat.

- [ ] Mobs « brain » : villageois (jobs, horaires, commerce `MERCHANT_OFFERS`), piglins (bartering), axolotls, grenouilles, renards
- [ ] Mobs volants restants : ghast, phantom (navigation volante + attaques), perroquet (perchoir)
- [ ] Mobs aquatiques : navigation sous-marine (squid, poissons, guardian, dauphin), trident du drowned
- [ ] Boss : Wither et Ender Dragon (`BOSS_EVENT`, combat multi-segment, invocations, drops)
- [ ] Mobs emblématiques : witch (potions), golems fer/neige, zombie villager, cheval/montures, ocelot/chat, panda,
      bee, golem de cuivre, allay
- [ ] Montures/apprivoisement étendus : chevaux, chats, perroquets, selles ; laisses (lead) ; loup : protéger son maître
      (`OwnerHurtByTargetGoal`), élevage, changement de couleur du collier
- [ ] Comportements : creeper chargé (foudre), araignées qui grimpent, conversion zombie→villageois, `CrossbowAttackGoal`
- [ ] Réglages de spawn : caps par dimension, spawn par biome (tableaux), `SpawnPlacements` étendus, groupes
- [ ] Sons par type (ambient/hurt/death/step) complets

## Villageois & commerce

- [ ] IA villageoise (professions, emplois, horaires, dormir)
- [ ] Commerce : `MERCHANT_OFFERS`, `SELECT_TRADE`, offres par profession et niveau
- [ ] Golem de fer, reproduction villageoise
- [ ] Viande de ravitaillement, moissons, stockage communautaire

## Boss

- [ ] Wither : invocation (3 têtes de wither + sable des âmes), `BOSS_EVENT`, attaques, drop du Nether Star
- [ ] Ender Dragon : combat de l'End, cristaux de l'End, `BOSS_EVENT`, porte de sortie
- [ ] Particules/sons propres aux boss, récompenses (dragon egg, gateway)

## Véhicules & montures

- [ ] Minecarts : `MOVE_MINECART_ALONG_TRACK`, `MOVE_VEHICLE`, rails, propulsion
- [ ] Bateaux : navigation, `PADDLE_BOAT`, `MOVE_VEHICLE`
- [ ] Chevaux et autres montures : apprivoisement, inventaire (`HORSE_SCREEN_OPEN`), selle
- [ ] Passagers : `SET_PASSENGERS`, `TELEPORT_ENTITY`, montée/descente
- [ ] Chariots de mine avec coffre / four / TNT

## Client : commandes & UI

- [ ] Panneaux éditables : `SIGN_UPDATE`, `OPEN_SIGN_EDITOR`
- [ ] Livres : `OPEN_BOOK`, `EDIT_BOOK`, `SET_PLAYER_INVENTORY` (livre dans l'inventaire)
- [ ] Recettes : `UPDATE_RECIPES` (le client connaît les recettes déverrouillées)
- [ ] Advancements : `UPDATE_ADVANCEMENTS`, `SELECT_ADVANCEMENTS_TAB`, déclencheurs de critères
- [ ] Statistiques : `AWARD_STATS`
- [ ] Scoreboard / teams : `SET_SCORE`, `SET_OBJECTIVE`, `SET_DISPLAY_OBJECTIVE`, `SET_PLAYER_TEAM`, `RESET_SCORE`
- [ ] World border : `INITIALIZE_BORDER`, `SET_BORDER_*`
- [ ] Cartes : `MAP_ITEM_DATA`, cartographie
- [ ] `SET_DEFAULT_SPAWN_POSITION` (compass qui pointe le spawn)
- [ ] Titles : `SET_TITLE_TEXT`, `SET_SUBTITLE_TEXT`, `SET_TITLES_ANIMATION`, `CLEAR_TITLES`, `SET_ACTION_BAR_TEXT`

## Client : effets visuels & audio

- [ ] Animation de minage : `BLOCK_DESTRUCTION` (fissures du bloc pendant le cassage)
- [ ] Sons d'entités : `SOUND_ENTITY`, `STOP_SOUND`
- [ ] Camera spectateur : `SET_CAMERA`, mode spectateur complet
- [ ] `PLAYER_ROTATION`, `PLAYER_LOOK_AT`
- [ ] Particules étendues : `LEVEL_PARTICLES` partiel, particles de bloc/explosion
- [ ] `EXPLODE` client (le calcul serveur existe) : sons, débris, onde de choc

## Réseau / divers

- [ ] `BLOCK_CHANGED_ACK` / ack de minage (partiel), `BLOCK_ENTITY_TAG_QUERY`
- [ ] `COOKIE_REQUEST`/`COOKIE_RESPONSE`, `CUSTOM_PAYLOAD` (plugin channels)
- [ ] Resource packs : `RESOURCE_PACK_PUSH`/`POP`, réponse du client
- [ ] Transfert de serveur : `TRANSFER`
- [ ] `PING_REQUEST`/`PONG_RESPONSE` play, `CLIENT_TICK_END`, `CHANGE_DIFFICULTY` client→serveur
- [ ] Chat sécurisé / signature : completion `DELETE_CHAT`, `DISGUISED_CHAT`
- [ ] `SET_SIMULATION_DISTANCE`, `SET_CHUNK_CACHE_RADIUS`

## Monde (hors générateur)

- [ ] Enregistrement du temps/weather dans `level.json` (persistance de la météo et de l'heure)
- [ ] Spawn conditionnel des mobs par biome (tableaux par type), caps par dimension
- [ ] Portails End : activation, plateforme, retour — à vérifier/compléter
- [ ] Climat : neige (blocage de la pluie), températures par biome appliquées
- [ ] Structuration `AnvilImporter` : biomes nether/end, block entities, entités importées
- [ ] Horloge de sauvegarde plus fine, sauvegarde des block entities (partiel)

## Qualité / infra

- [ ] Corriger `make debug` : `-DDEBUG` (Makefile) entre en collision avec `enum LogLevel { DEBUG, ... }`
      (`include/logger.hpp:13`) → renommer l'enum ou le flag
- [ ] Régénérer `docs/PACKETS_SUPPORTED.md` / `docs/PACKETS_MISSING.md` (`make packets`) : EXplode etc. déjà envoyés
- [ ] Tests unitaires couvrant les nouvelles commandes (`tests/`)
- [ ] Revoir les packets déclarés mais non documentés dans les docs