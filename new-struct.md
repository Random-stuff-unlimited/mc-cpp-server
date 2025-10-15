# Enhanced Chunk Data Structure Design

## Overview

This document outlines the design for a high-performance chunk data structure optimized for Minecraft server implementation. The new architecture eliminates intermediate conversions and provides direct NBT-to-network serialization.

## Architecture Goals

- **Zero-Copy Operations**: Direct parsing from NBT to network format
- **Memory Efficiency**: Optimal palette encoding and bit packing
- **Protocol Compliance**: Full compatibility with Minecraft 1.21.5+ protocol
- **Performance**: Minimal allocations and fast serialization
- **Extensibility**: Support for custom world heights and future protocol changes

## Core Components

### 1. Paletted Container System

The paletted container handles efficient storage of repetitive data (blocks, biomes) using three encoding strategies:

- **Single Valued**: All entries are identical (0 bits per entry)
- **Indirect**: Small palette with indices (1-8 bits for blocks, 1-3 for biomes)
- **Direct**: Raw values (15 bits for blocks, 6 for biomes)

```cpp
class PalettedContainer {
public:
    enum class Type {
        SINGLE_VALUED = 0,
        INDIRECT = 1,
        DIRECT = 2
    };

private:
    Type _type;
    uint8_t _bitsPerEntry;
    std::vector<uint32_t> _palette;
    std::vector<uint64_t> _data;
    size_t _size;
    bool _isBlockContainer;

public:
    explicit PalettedContainer(size_t size);
    
    void setSingleValue(uint32_t value);
    void setFromArray(const std::vector<uint32_t>& values);
    uint32_t getValue(size_t index) const;
    std::vector<uint8_t> serialize() const;
};
```

### 2. Height Map Handler

Manages multiple heightmap types with proper bit packing according to Minecraft specifications:

```cpp
class HeightMapHandler {
public:
    enum class Type {
        MOTION_BLOCKING = 1,
        MOTION_BLOCKING_NO_LEAVES = 2,
        OCEAN_FLOOR = 3,
        WORLD_SURFACE = 4,
        WORLD_SURFACE_WG = 5
    };

private:
    std::map<Type, std::vector<uint16_t>> _heightmaps;
    int _worldHeight;
    int _minY;

public:
    HeightMapHandler(int worldHeight = 384, int minY = -64);
    
    void setHeightMap(Type type, const std::vector<int64_t>& packedData);
    void setHeightMapDirect(Type type, const std::vector<uint16_t>& heights);
    void generateEmpty();
    std::vector<uint8_t> serializeToNBT() const;
};
```

### 3. Chunk Section Structure

Represents a 16x16x16 section of blocks with associated data:

```cpp
struct ChunkSection {
    uint16_t blockCount = 0;
    std::unique_ptr<PalettedContainer> blockStates; // 4096 entries
    std::unique_ptr<PalettedContainer> biomes;      // 64 entries
    
    std::vector<uint8_t> skyLight;    // 2048 bytes
    std::vector<uint8_t> blockLight;  // 2048 bytes
    
    bool hasSkyLight = false;
    bool hasBlockLight = false;
    bool isEmpty = true;

    ChunkSection();
    
    void calculateBlockCount(const std::vector<uint32_t>& blockStateIds);
    void setBlocks(const std::vector<uint32_t>& blockStateIds);
    void setBiomes(const std::vector<uint32_t>& biomeIds);
    void initializeLighting(bool withSkyLight = true, bool withBlockLight = false);
    std::vector<uint8_t> serialize() const;
};
```

### 4. Enhanced Chunk Data

Complete chunk representation with all necessary components:

```cpp
struct EnhancedChunkData {
    int32_t chunkX, chunkZ;
    
    HeightMapHandler heightmaps;
    std::vector<ChunkSection> sections;
    
    struct BlockEntity {
        uint8_t packedXZ;                // (x & 15) << 4 | (z & 15)
        int16_t y;
        uint32_t type;
        std::vector<uint8_t> nbtData;
        
        BlockEntity(uint8_t x, uint8_t z, int16_t yPos, uint32_t typeId);
    };
    std::vector<BlockEntity> blockEntities;
    
    bool isFullyGenerated = false;
    int64_t inhabitedTime = 0;
    int64_t lastUpdate = 0;

    EnhancedChunkData(int32_t x, int32_t z, int worldHeight = 384, int minY = -64);
    
    ChunkSection* getSectionByY(int worldY, int minY = -64);
    bool isEmpty() const;
    
    static EnhancedChunkData generateEmpty(int32_t x, int32_t z, int worldHeight = 384);
};
```

## Direct Parsing Architecture

### 5. Direct Chunk Parser

Eliminates intermediate conversions by parsing directly from NBT to enhanced structure:

```cpp
class DirectChunkParser {
public:
    struct ParseConfig {
        int worldHeight = 384;
        int minY = -64;
        bool loadLighting = true;
        bool optimizePalettes = true;
    };

private:
    ParseConfig _config;

public:
    DirectChunkParser(const ParseConfig& config = {});
    
    EnhancedChunkData parseChunkFromRegion(
        const std::filesystem::path& regionPath, 
        int32_t chunkX, 
        int32_t chunkZ
    );
    
    EnhancedChunkData parseChunkFromNBT(
        const nbt::NBT& chunkNBT,
        int32_t chunkX,
        int32_t chunkZ
    );

private:
    void parseHeightMaps(const nbt::TagCompound& heightmaps, EnhancedChunkData& chunk);
    void parseSections(const nbt::TagList& sections, EnhancedChunkData& chunk);
    void parseBlockStates(const nbt::TagCompound& blockStates, ChunkSection& section);
    void parseBiomes(const nbt::TagCompound& biomes, ChunkSection& section);
    
    std::vector<uint32_t> unpackPalettedData(
        const nbt::TagLongArray& data,
        const nbt::TagList* palette,
        size_t expectedSize,
        uint32_t defaultValue = 0
    );
};
```

### 6. Optimized World Query

High-performance chunk loading with caching support:

```cpp
namespace World {

class OptimizedQuery {
private:
    Manager& _worldManager;
    DirectChunkParser _parser;
    std::unordered_map<uint64_t, std::shared_ptr<EnhancedChunkData>> _chunkCache;

public:
    explicit OptimizedQuery(Manager& manager);
    
    EnhancedChunkData fetchChunkDirect(int32_t chunkX, int32_t chunkZ);
    std::shared_ptr<EnhancedChunkData> fetchChunkCached(int32_t chunkX, int32_t chunkZ);
    
    std::vector<EnhancedChunkData> fetchChunkBatch(
        const std::vector<std::pair<int32_t, int32_t>>& coordinates
    );
    
    void clearCache();
};

} // namespace World
```

## Network Protocol Integration

### 7. Packet Handler

Optimized packet creation with direct serialization:

```cpp
class LevelChunkWithLightPacket {
private:
    static constexpr uint32_t PACKET_ID = 0x27;

public:
    static void send(Packet& packet, int32_t chunkX, int32_t chunkZ, Server& server);

private:
    static void writeLightData(Buffer& buffer, const EnhancedChunkData& chunk);
};
```

## Performance Benefits

### Memory Efficiency
- **Palette Optimization**: Automatic selection of most efficient encoding
- **Bit Packing**: Optimal use of memory for height maps and light data
- **Zero Copies**: Direct NBT to network conversion

### Processing Speed
- **Single Pass**: Parse NBT directly to final structure
- **Batch Loading**: Efficient I/O for multiple chunks
- **Lazy Evaluation**: Load sections only when needed

### Network Optimization
- **Protocol Compliance**: Exact adherence to Minecraft packet format
- **Compression Ready**: Structures optimized for network transmission
- **Light Data Masking**: Efficient sky/block light handling

## Migration Strategy

1. **Phase 1**: Implement core structures (PalettedContainer, HeightMapHandler)
2. **Phase 2**: Create DirectChunkParser and integrate with existing NBT system
3. **Phase 3**: Replace packet handlers to use enhanced structures
4. **Phase 4**: Add caching and batch loading optimizations

## Extensibility Features

- **Custom World Heights**: Support for modded dimensions
- **Additional Heightmaps**: Easy addition of new heightmap types
- **Block Entity Extensions**: Flexible NBT data handling
- **Version Compatibility**: Designed for future protocol changes

## Implementation Notes

- All bit operations follow Minecraft's little-endian conventions
- Light data uses 4-bit nibbles packed into bytes
- Palette indices are packed using variable bit widths
- Height maps use ceil(log2(world_height + 1)) bits per entry
- Block entities store coordinates in packed format for network efficiency