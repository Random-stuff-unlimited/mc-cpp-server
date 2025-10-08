Title: Chunk Streaming, Caching, and World-Sync Subsystem (Spec Sheet)
Version: 0.1
Owner: Antoine (Server Runtime)
Status: Draft
Last Updated: 2025-10-08

1) Goals
- Stream chunks from Anvil region files on demand, decode via libnbt++, and
  maintain a bounded in-memory cache.
- Keep main server loop authoritative over simulation (entities, blocks, light).
- Maintain consistency between background I/O/decoding and live world state.
- Pin spawn and simulation-radius chunks; prefetch neighbors; evict by LRU/TTL.
- Support async load and async save (writeback) without stalling the tick.
- Provide deterministic activation/eviction phases in the tick.

2) Non-Goals
- Procedural generation (assume existing Anvil data).
- Full lighting algorithm specification (only interfaces/hooks).
- Network protocol specifics.

3) Terminology
- ChunkPos: integer pair {x, z} in chunk coordinates (16×16 blocks each).
- Region: 32×32 chunks per .mca file (r.<rx>.<rz>.mca).
- Snapshot (immutable): decoded NBT data for a chunk (sections, biomes,
  entities list, etc.), safe to move across threads.
- LiveChunk (mutable): runtime structures used by simulation (blocks, entities,
  tile entities, light caches, pathing data).
- ActiveSet: map of ChunkPos -> LiveChunk under main-thread ownership.
- DesiredSet: union of spawn + per-player simulation radius + optional
  prefetch radius.
- CacheEntry: background cache record (may hold Snapshot or nothing).

4) Components
4.1) RegionFile (I/O)
- Responsibility: Read compressed chunk blobs from .mca files.
- Interfaces:
  - static path regionPathFor(worldRoot, cx, cz) -> filesystem::path
  - ctor(path)
  - bool ok()
  - optional<vector<uint8_t>> readChunk(cx, cz)
  - (Future) writeChunk(cx, cz, compressedBlob) [async via Writeback]
- Behavior:
  - Parse 8 KiB header (location + timestamps).
  - readChunk returns [length:4][compression:1][payload...] as stored.
  - Keeps file descriptor open; thread-safe via external serialization (one
    RegionFile instance per region, methods may be called from thread pool).
- Errors: returns nullopt for missing chunks; throws only on fatal open errors.

4.2) ChunkCodec (Decode/Encode)
- Responsibility: Convert between compressed blob and Snapshot.
- Interfaces:
  - decodeFromCompressedNbt(ChunkPos, blob) -> Snapshot + stats
  - encodeToCompressedNbt(ChunkPos, Snapshot) -> blob + stats
- Behavior:
  - Supports compression types: 2=Zlib/Deflate, 1=Gzip, 3=LZ4 (if present).
  - Uses libnbt++ to parse NBT tags; maps to Snapshot schema.
  - Memory accounting: reports uncompressed size for budgeting.
- Errors: returns null/empty Snapshot on malformed input; logs diagnostics.

4.3) ChunkManager (Background cache + async orchestration)
- Responsibility: Manage memory-bounded cache of Snapshots and orchestrate
  background I/O and decoding/encoding.
- Interfaces:
  - getChunkAsync(pos) -> future<Snapshot>
  - prefetch(pos) -> void (low-priority get)
  - pin(pos), unpin(pos) -> void
  - saveAsync(pos, Snapshot) -> future<void>
  - tick() -> void (eviction by LRU/TTL)
  - setMemoryBudget(bytes) -> void
  - metrics() -> struct { usedBytes, entries, pinned, inflightLoads, inflightSaves, p50/p95 IO+decode }
- Policies:
  - Eviction: LRU with TTL; skip pinned and inflight; drop unpinned first.
  - Memory budget: total bytes ~ sum of Snapshot size estimates.
  - Prefetch: queued after required loads; dropped first under pressure.
- Concurrency:
  - Thread pool(s) for I/O and decode/encode.
  - No main-thread blocking except when caller chooses to wait on futures.
- Errors: futures resolve to empty Snapshot on failure; callers decide fallback.

4.4) WorldState (Main-thread authoritative)
- Responsibility: Owns and mutates LiveChunks; integrates Snapshots deterministically.
- Interfaces:
  - ensureActive(pos) -> schedules load if needed (non-blocking)
  - deactivate(pos) -> schedules save if dirty, then unload
  - getLive(pos) -> LiveChunk* or null
  - iterateActive(aabb or radius) -> iterator
  - markDirty(pos) -> void
  - buildLiveFromSnapshot(pos, Snapshot) -> LiveChunk
  - encodeSnapshot(LiveChunk) -> Snapshot (for save)
- Data:
  - ActiveSet: unordered_map<ChunkPos, LiveChunk>
  - PendingLoads: map<ChunkPos, future<Snapshot>>
  - PendingActivations: queue<(ChunkPos, Snapshot)>
  - PendingEvictions: queue<ChunkPos>
  - WritebackQueue: queue<(ChunkPos, Snapshot)>
- Policies:
  - Activation rate limit per tick to bound frame spikes (e.g., 16).
  - Defer cross-chunk operations until neighbors are present if required.

4.5) EntitySystem and Subsystems
- Responsibility: Simulate only within ActiveSet.
- Interfaces:
  - onTick(WorldView view) where view queries LiveChunks only.
  - TransferQueue for entities crossing chunk borders pending activation.
- Guarantees:
  - Never reads background Snapshots directly.
  - Handles “chunk not loaded” by deferring or scheduling prefetch.

5) Tick Phases (Deterministic Order)
- Phase 0: Input & Interest
  - Compute DesiredSet from players (Rs sim radius, Rd prefetch radius) + spawn.
  - For each pos in DesiredSet:
    - If not active and not pending: ChunkManager.pin(pos); pendingLoads[pos] =
      getChunkAsync(pos). For prefetch-only area: do not pin.
  - For each pos in ActiveSet not in DesiredSet and not dirty: enqueue eviction.
- Phase 1: Drain Async (Non-blocking)
  - Poll PendingLoads with wait_for(0ms).
  - Move ready results to PendingActivations; remove from PendingLoads.
- Phase 2: World Sync
  - Activate up to N per tick: buildLiveFromSnapshot; insert into ActiveSet.
  - Process PendingEvictions: if dirty -> encode -> saveAsync; unpin; erase.
  - Resolve edge dependencies (e.g., light border fix) if neighbors present.
- Phase 3: Simulation
  - Run systems in fixed order on ActiveSet (blocks, tiles, AI, physics, entities).
  - Process transfers across chunk borders; if target inactive, queue for next tick.
- Phase 4: Output & Maintenance
  - Produce network snapshots from ActiveSet only.
  - ChunkManager.tick(); flush writebacks opportunistically.

6) Memory and Performance
- Budgets:
  - Cache budget: default 512 MiB (configurable).
  - Activation cap per tick: default 16 (configurable).
  - Prefetch radius Rd = Rs + 2..4; disabled under pressure.
- Estimation:
  - Snapshot.memoryBytes = sum(capacities) for sections, entities, biomes, light.
- Eviction Order:
  - Prefetch-only unpinned -> old LRU unpinned -> TTL-expired -> never pinned.
- Backpressure:
  - If usedBytes > budget: stop new prefetch; evict until under budget.
  - If still high: reduce Rd; as a last resort, throttle new required loads (warn).

7) Threading and Safety
- Background threads: I/O, decode, encode only; they never touch LiveChunks.
- Main thread: exclusive owner of ActiveSet and LiveChunks.
- Handoffs:
  - Background produces Snapshot (immutable).
  - Main thread consumes Snapshot and creates LiveChunk.
- Locks:
  - ChunkManager internal locks around maps/LRU and region handle cache.
  - WorldState is single-threaded; no locks on ActiveSet.
- Futures:
  - Never block the tick loop; poll ready futures only.

8) Error Handling and Recovery
- Missing chunk in region: Snapshot=null -> generator hook or treat as void.
- Corrupt NBT: log warning with pos and region; skip activation.
- Save failure: retry with backoff; mark chunk “savePending”; keep pinned until
  persisted or max retries reached (then escalate).
- Region open failure: mark region offline; degrade gracefully; alert.

9) Instrumentation and Metrics
- Expose counters/gauges:
  - activeChunks, pendingLoads, pendingActivations, pendingEvictions
  - cacheUsedBytes/currentBudget, pinnedCount, inflightLoads/saves
  - IO latency p50/p95, decode time p50/p95, activation time p50/p95
- Tracing spans:
  - readChunk, decode, encode, writeChunk, activate, evict
- Logs:
  - INFO on region opens/closes; WARN on decode/save errors; DEBUG on evictions.

10) Configuration
- CacheConfig:
  - memoryBudgetBytes (default 512 MiB)
  - ttlSeconds (default 120)
  - enableTTL (default true)
- Runtime:
  - ioThreads (default 2), decodeThreads (default 2), encodeThreads (default 2)
  - simRadius Rs per world or per player (default 8)
  - prefetchRadius Rd relative to Rs (default +2)
  - maxActivationsPerTick (default 16)
  - saveBatchSize (default 8)
- Paths:
  - worldRoot pointing to Anvil world directory (contains region/).

11) Data Schemas (Abstract)
- Snapshot:
  - ChunkPos pos
  - int dataVersion
  - vector<Section> sections (Y-indexed; palette + blockStates bits)
  - Biome data (per-column or per-section)
  - Heightmap(s)
  - Entities: vector<EntityData> (raw NBT fields or parsed minimal)
  - TileEntities: vector<BlockEntityData>
  - Light (optional raw arrays if read)
  - size_t memoryBytes
- LiveChunk:
  - ChunkPos pos
  - Blocks (mutable accessor over sections)
  - Entities (engine-owned IDs)
  - TileEntities
  - Light caches
  - Dirty flags: blocks/entities/tileEntities
  - Aux: navigation mesh, scheduled ticks
- CacheEntry:
  - optional<Snapshot> snapshot
  - future<Snapshot> inflightLoad
  - future<void> inflightSave
  - bool pinned
  - time_point lastUsed
  - size_t sizeBytes

12) APIs (C++ Signatures, indicative)
- ChunkManager
  - future<Snapshot> getChunkAsync(ChunkPos pos);
  - void prefetch(ChunkPos pos);
  - void pin(ChunkPos pos);
  - void unpin(ChunkPos pos);
  - future<void> saveAsync(ChunkPos pos, const Snapshot& s);
  - void tick();
  - void setMemoryBudget(size_t bytes);
  - Metrics metrics() const;
- WorldState
  - void requestEnsureActive(ChunkPos pos);
  - void processAsyncLoadsNonBlocking(); // moves ready into PendingActivations
  - void activateSome(size_t maxCount);
  - void scheduleDeactivate(ChunkPos pos);
  - LiveChunk* getLive(ChunkPos pos);
  - void markDirty(ChunkPos pos, DirtyMask mask);
  - Snapshot encodeSnapshot(const LiveChunk& lc) const;

13) Edge Policies
- Neighbor-dependent systems (lighting, fluid flow, redstone):
  - Allow partial activation with border-deferred mode; reconcile when both
    neighbors are active; maintain a “border dirty” mask per face.
- Cross-chunk entity movement:
  - If target chunk inactive: queue transfer; ensure target pos in DesiredSet.
- Spawn area:
  - Permanently pinned unless server config unpins during high pressure.

14) Testing Strategy
- Unit:
  - Region header parse; readChunk for known fixtures.
  - Codec decode/encode roundtrip for multiple DataVersion samples.
  - LRU/TTL eviction order under budget change.
- Integration:
  - Simulate player walk; assert ActiveSet sliding window; no blocking.
  - Memory cap adherence under synthetic pressure.
  - Save/load dirty chunk with concurrent evictions.
- Soak:
  - Long-running walk across regions; capture latency histograms.

15) Migration/Compatibility
- Support multiple DataVersions by feature flags in ChunkCodec.
- Backward-compatible save: preserve unknown NBT tags when round-tripping.

16) Security/Robustness
- Validate NBT lengths vs. header to avoid OOM.
- Cap per-chunk uncompressed size (e.g., 32 MiB) and reject if exceeded.
- Limit inflight loads and activations per tick to avoid spikes.

17) Future Work
- Compression dictionary caching; in-memory section-level compression.
- Hotset prioritization via LFU or ARC instead of pure LRU.
- NUMA-aware thread pools and file read-ahead/batching.
- Sharded world loops for multi-core scaling (region-based ownership).
S
