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
`time`, `list`, `setblock`, `give`, `clear`, `kill`, `difficulty`, `weather`, `summon`, `effect`, `xp`, `enchant`, et
les commandes d'admin ci-dessous. Sans être op, seules `help`, `list`, `spawn`, `tell`/`msg`/`w` et `me` sont permises.

- [x] Admin : `/op`, `/deop`, `/kick`, `/ban`, `/ban-ip`, `/pardon`, `/pardon-ip`, `/banlist`, `/whitelist`, `/say`,
      `/tell` (`/msg`, `/w`), `/me` ; niveaux de permission 0–4 (arbre de commandes, `/help` et suggestions filtrés),
      `ops.json`, `banned-players.json`, `banned-ips.json`, `whitelist.json` (format vanilla, à côté de `config.json`),
      refus au login ; `config.json` : `op-permission-level`, `white-list`, `enforce-whitelist`
- [x] Console serveur (stdin, niveau 4, messages en anglais dans le log), `/stop`, `/save-all`
- [ ] Bans temporaires ; historique et complétion dans la console
- [x] `/summon <entité> [x y z]`
- [x] `/effect give|clear`
- [x] `/xp` (`/experience`) `add|set|query`, en points ou niveaux
- [x] `/enchant <joueurs> <enchantement> [niveau]` (objets supportés, niveau max, incompatibilités)
- [x] `/weather <type> [durée]`, `/time set|add <durée>` avec unités (d, s, t), durées vanilla aléatoires par défaut
- [ ] Autocomplétion des commandes côté client (`COMMANDS` + `COMMAND_SUGGESTIONS`, `Commands::suggest` existe déjà)

## Effets de statut / potions

- [x] Moteur d'effets de statut (vitesse, force, régénération, poison…), durée et niveaux
- [x] Envoyer les effets au client : `UPDATE_MOB_EFFECT`, `REMOVE_MOB_EFFECT`, `ENTITY_EVENT` (guérison)
- [x] Application à l'entité : `LivingEntity.addEffect`, tick des effets, dégâts périodiques (poison, warding…)
- [x] Effets des potions bues (`ItemUse` → effet), potions jetables et persistantes (nuage d'effet), flèches à effets
- [x] Défenses/mitigations liées aux effets (résistance, absorption, résistance au feu)
- [x] Beacon : pyramide (1–4 niveaux), faisceau, `SET_BEACON`, effets du beacon, coût en items

## Mobs & IA

Les 85 types de mobs vanilla ont leur classe et leur IA (`src/world/entity/mobs`, enregistrés dans
`src/world/entity/ai/MobGoals.cpp`) ; un test les fait tous spawner, tourner, sauvegarder et recharger. Les mobs à
« Brain » vanilla sont approchés par des goals. Infra : goals, pathfinding sol/eau/amphibie/**vol** (`FlyNodeEvaluator`,
`FlyingPathNavigation`, `FlyingMoveControl`, `Entity::setNoGravity`), `RangedCrossbowAttackGoal` partagé, barre de boss
(`BossBar`, `BOSS_EVENT`), ids consécutifs pour les entités à parties (`IdManager::allocateRange`).

- [x] Monstres : zoglin, giant, hoglin (animal, zombifie hors du Nether), breeze (charges de vent, sauts), creaking
      (figé quand on le regarde), shulker (balles à lévitation, téléportation), warden (vibrations, colère, rayon sonique,
      obscurité, émerge/creuse)
- [x] Illagers : vindicator, pillager (arbalète), evoker (crocs, vex, wololo), illusioner, vex, ravager
- [x] Animaux : llama/trader llama (crachat), camel (s'assoit, selle sans dressage), goat (coup de tête), parrot,
      bee (pollinisation, miel, cultures, piqûre), armadillo, sniffer (graines anciennes, œuf), frog/tadpole, axolotl
- [x] Aides : allay (ramène les objets), golem de cuivre (trie les coffres de cuivre, oxydation), happy ghast (harnais)
- [x] Piglins (troc, or, arbalète/épée, zombification), piglin brute
- [x] Villageois (métier par poste de travail, lit, emploi du temps, sommeil, fuite, reproduction), wandering trader
- [x] Boss : wither (invocation par motif, 3 têtes, crânes, barre de boss), ender dragon (phases de vol, charge,
      boules de feu, atterrissage, mort, parties)
- [x] Commerce villageois : `MERCHANT_OFFERS`, `SELECT_TRADE`, menu marchand, offres des 13 métiers et du marchand ambulant,
      demande et réapprovisionnement, XP et niveaux de métier (cartes d'exploration, armures teintes, ragots non portés)
- [x] Villageois : invocation de golems de fer (panique, rencontres après avoir dormi), conversion par les zombies
- [x] Fermiers : récolte, replantation, pain (3 blés), graines au composteur
- [ ] Villageois : vrais ragots (réputation), partage de nourriture entre villageois
- [x] Éclosion des œufs (frai de grenouille → têtards, œuf de renifleur ; tortue déjà en place)
- [x] Ruches : abeilles qui entrent/sortent (`BeehiveBlockEntity`), nuit/pluie, miel à la sortie, essaim en colère si cassée
- [x] Ruches : fumée de feu de camp, récolte au flacon/cisailles (abeilles en colère sans fumée)
- [x] Cristaux de l'End, combat de l'End (piliers, cages, portail de sortie, œuf, `end_fight.dat`)
- [x] Dragon : réinvocation avec 4 cristaux (piliers reconstruits), portails de passage (20, vers les îles à 1024 blocs, retour)
- [ ] Dragon : dégâts par partie touchée ; îles extérieures (pas de générateur de l'End)
- [x] Spawners : marchand ambulant, patrouilles (capitaine à bannière), raids (Mauvais présage → Présage de raid →
      vagues, barre de raid, Héros du village), golems par motif (fer/neige/cuivre)
- [ ] Raids : fiole sinistre (le capitaine donne directement Mauvais présage ici), bannière sinistre à motifs, laisses
      des lamas du marchand, état des spawners non sauvegardé
- [x] Inventaire des chevaux/ânes/mules/lamas (`HORSE_SCREEN_OPEN`) : selle, armure/tapis, coffre ; laisses (tirer,
      casser à 10 blocs, nœud sur barrière, `SET_ENTITY_LINK`)
- [x] Perroquets sur l'épaule (données 19/20, s'envolent en tombant, dans l'eau, en volant, en dormant, blessé, à la déconnexion)
- [ ] Caravanes de lamas
- [x] Foudre : éclairs d'orage (1/100000 par chunk et par tick), paratonnerres (attirent, signal 8 ticks), feu, creeper
      chargé, cochon → piglin zombifié, villageois → sorcière, mooshroom, tortue ; `/summon lightning_bolt`
- [x] Foudre : piège du cheval squelette (squelettes à côté des chevaux : pas de mobs cavaliers), cuivre désoxydé
- [ ] Comportements : danse des piglins/allays (jukebox), duplication d'allay, piglins chasseurs
      de hoglins et colère aux coffres ouverts, cornes de chèvre, sauts longs (chèvre, grenouille), dash du chameau
- [x] Réglages de spawn : spawn par biome (tableaux de `worldgen/biome`, coûts de spawn), caps globaux et par joueur,
      caps configurables par dimension (`config.json` → `world.spawn-limits`)
- [ ] Spawns liés aux structures (forteresse, cabane de sorcière, monument, avant-poste) : pas de structures générées
- [x] Sons : ambiance/blessure/mort par type, pas des mobs (le leur ou celui du bloc), pose de bloc entendue par les autres
- [ ] Types de son exacts des blocs (ici déduits du nom : le gamedata ne les a pas)

## Villageois & commerce

- [x] IA villageoise (professions, emplois, horaires, dormir)
- [x] Commerce : `MERCHANT_OFFERS`, `SELECT_TRADE`, offres par profession et niveau
- [x] Golem de fer construit par motif
- [x] Golem de fer invoqué par les villageois
- [x] Fermiers : moissons
- [x] Composteur : table de compostage vanilla, niveaux, poudre d'os (joueurs et fermiers)
- [x] Composteur : entonnoirs (remplir par le haut, poudre d'os par le bas)

## Boss

- [x] Wither : invocation (3 têtes de wither + sable des âmes), `BOSS_EVENT`, attaques, drop du Nether Star (loot table)
- [x] Ender Dragon : vol, charge, boules de feu, atterrissage, `BOSS_EVENT`, mort
- [x] Ender Dragon : cristaux de l'End, combat de l'End, portail de sortie, œuf
- [x] Souffle persistant du dragon (nuage d'effet de zone), fioles de souffle

## Véhicules & montures

- [x] Minecarts : rails (courbes, pentes), rails de propulsion/détecteur/activateur, `MOVE_VEHICLE`
- [ ] Minecarts : nouvelle physique expérimentale (`MOVE_MINECART_ALONG_TRACK`), comparateurs sur les rails détecteurs
- [x] Bateaux et bateaux à coffre : flottaison, `PADDLE_BOAT`, `MOVE_VEHICLE`, casse et drop
- [x] Chevaux et autres montures : apprivoisement, inventaire (`HORSE_SCREEN_OPEN`), selle
- [x] Passagers : montée/descente (un seul passager)
- [x] Plusieurs passagers par véhicule (bateau 2, chameau 2, happy ghast 4), le premier dirige
- [ ] Mobs passagers (jockeys, cavaliers squelettes)
- [x] Chariots de mine avec coffre / four / TNT / entonnoir

## Client : commandes & UI

- [x] Panneaux éditables (et suspendus) : pose sol/mur/plafond, `SIGN_UPDATE`, `OPEN_SIGN_EDITOR`, teintures, encre lumineuse, cire
- [x] Livres : `EDIT_BOOK` (édition, signature → livre écrit), `OPEN_BOOK`
- [ ] Recettes : `UPDATE_RECIPES` (le client connaît les recettes déverrouillées)
- [ ] Advancements : `UPDATE_ADVANCEMENTS`, `SELECT_ADVANCEMENTS_TAB`, déclencheurs de critères
- [x] Statistiques : `AWARD_STATS` (écran Statistiques), `stats/<uuid>.json`, temps de jeu, distances, sauts, morts,
      kills, blocs minés, objets ramassés/jetés/posés, mobs tués / tué par
- [ ] Statistiques : objets fabriqués, outils cassés, dégâts infligés/subis, interactions (coffres, fours…)
- [x] Scoreboard : `/scoreboard` (objectifs, scores, affichage), `SET_SCORE`, `SET_OBJECTIVE`, `SET_DISPLAY_OBJECTIVE`,
      `RESET_SCORE`, critères deathCount/playerKillCount/totalKillCount/health/food/level/xp/air/armor, `scoreboard.json`
- [x] Équipes : `/team` (couleur, préfixe/suffixe, tir allié, pseudos, collision), `SET_PLAYER_TEAM`, sauvegarde
- [ ] `/scoreboard players operation`, `/trigger`, collisions d'équipe côté serveur
- [x] World border : `INITIALIZE_BORDER`, `SET_BORDER_*`, `/worldborder`, dégâts hors limite, blocs interdits, sauvegarde
- [ ] Cartes : `MAP_ITEM_DATA`, cartographie
- [x] `SET_DEFAULT_SPAWN_POSITION` (compass qui pointe le spawn)
- [x] Titles : `SET_TITLE_TEXT`, `SET_SUBTITLE_TEXT`, `SET_TITLES_ANIMATION`, `CLEAR_TITLES`, `SET_ACTION_BAR_TEXT`, commande `/title` (texte brut ou JSON)

## Client : effets visuels & audio

- [x] Animation de minage : `BLOCK_DESTRUCTION` (fissures pour les autres joueurs, vitesse de minage vanilla)
- [ ] Anti-triche du minage (temps de cassage vérifié au `FINISH_DIGGING`)
- [x] `STOP_SOUND` (`/stopsound`), `/playsound`
- [ ] `SOUND_ENTITY` (sons qui suivent une entité)
- [ ] Camera spectateur : `SET_CAMERA`, mode spectateur complet
- [x] `PLAYER_LOOK_AT` (`/rotate … facing`), `/rotate`
- [ ] Particules étendues : `LEVEL_PARTICLES` partiel, particles de bloc/explosion
- [ ] `EXPLODE` client (le calcul serveur existe) : sons, débris, onde de choc

## Réseau / divers

- [ ] `BLOCK_CHANGED_ACK` / ack de minage (partiel), `BLOCK_ENTITY_TAG_QUERY`
- [ ] `COOKIE_REQUEST`/`COOKIE_RESPONSE`, `CUSTOM_PAYLOAD` (plugin channels)
- [x] Resource packs : `RESOURCE_PACK_PUSH` (config.json `resource-pack`, `require-resource-pack`), refus → expulsion
- [x] Transfert de serveur : `/transfer`, `TRANSFER`, transferts entrants (`accepts-transfers`)
- [x] `PING_REQUEST`/`PONG_RESPONSE` play, `CLIENT_TICK_END`, `CHANGE_DIFFICULTY`, `LOCK_DIFFICULTY`, `CHANGE_GAME_MODE` (F3+F4),
      `BLOCK_ENTITY_TAG_QUERY`/`ENTITY_TAG_QUERY` (F3+I), `PICK_ITEM_FROM_ENTITY`, `TELEPORT_TO_ENTITY`, `CUSTOM_PAYLOAD` (brand)
- [ ] Chat sécurisé / signature : completion `DELETE_CHAT`, `DISGUISED_CHAT`
- [ ] `SET_SIMULATION_DISTANCE`, `SET_CHUNK_CACHE_RADIUS`

## Monde (hors générateur)

- [x] Enregistrement du temps/weather dans `level.json` (persistance de la météo et de l'heure)
- [x] Spawn conditionnel des mobs par biome (tableaux par type), caps par dimension
- [ ] Portails End : activation, plateforme, retour — à vérifier/compléter
- [x] Climat : neige qui tient et eau qui gèle dans les biomes froids, chaudrons remplis par la pluie/neige, température selon l'altitude
- [ ] Structuration `AnvilImporter` : biomes nether/end, block entities, entités importées
- [ ] Horloge de sauvegarde plus fine, sauvegarde des block entities (partiel)

## Qualité / infra

- [x] Tests reproductibles : graine du monde et hasard du niveau fixés dans `LevelFixture`, hasard des entités tiré de
      celui du niveau
- [x] `make debug` : le flag est devenu `-DMC_DEBUG` (`-DDEBUG` cassait l'enum `LogLevel`) ; `make clean` pour changer de mode
- [ ] Régénérer `docs/PACKETS_SUPPORTED.md` / `docs/PACKETS_MISSING.md` (`make packets`) : EXplode etc. déjà envoyés
- [ ] Tests unitaires couvrant les nouvelles commandes (`tests/`)
- [ ] Revoir les packets déclarés mais non documentés dans les docs