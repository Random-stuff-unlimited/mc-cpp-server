#include "world.hpp"
#include "world/chunkParser.hpp"

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace World {

	/**
	 * OptimizedQuery - High-performance chunk loading with built-in performance tracking
	 *
	 * Features:
	 * - Direct NBT to enhanced chunk structure parsing
	 * - LRU-style chunk caching with configurable size
	 * - Batch loading for multiple chunks grouped by region
	 * - Zero-copy operations where possible
	 * - Built-in performance metrics and monitoring
	 * - Automatic cache management and eviction
	 */
	class OptimizedQuery {
	  public:
		// Performance metrics structure
		struct PerformanceMetrics {
			size_t totalQueries	   = 0;
			size_t cacheHits	   = 0;
			size_t cacheMisses	   = 0;
			double totalLoadTime   = 0.0;
			double averageLoadTime = 0.0;
			size_t batchQueries	   = 0;
			double lastQueryTime   = 0.0;
		};

	  private:
		Manager&												 _worldManager;
		std::unique_ptr<DirectChunkParser>						 _parser;
		std::unordered_map<uint64_t, std::shared_ptr<ChunkData>> _chunkCache;

		// Performance tracking
		mutable PerformanceMetrics _metrics;
		bool					   _enableMetrics = true;

	  public:
		explicit OptimizedQuery(Manager& manager);

		// Core chunk loading methods
		ChunkData				   fetchChunkDirect(int32_t chunkX, int32_t chunkZ);
		std::shared_ptr<ChunkData> fetchChunkCached(int32_t chunkX, int32_t chunkZ);

		// Batch operations for efficiency
		std::vector<ChunkData> fetchChunkBatch(const std::vector<std::pair<int32_t, int32_t>>& coordinates);

		// Cache management
		void   clearCache();
		size_t getCacheSize() const;
		bool   isChunkCached(int32_t chunkX, int32_t chunkZ) const;
		void   evictChunk(int32_t chunkX, int32_t chunkZ);

		// Preloading for performance
		void preloadChunks(const std::vector<std::pair<int32_t, int32_t>>& coordinates);

		// Parser configuration
		struct ParseConfig {
			int	   worldHeight		 = 384;
			int	   minY				 = -64;
			bool   loadLighting		 = true;
			bool   optimizePalettes	 = true;
			bool   validateData		 = true;
			bool   loadBlockEntities = true;
			bool   useMemoryMapping	 = true;
			size_t cacheSize		 = 64;
		};

		ParseConfig getParserConfig() const;
		void		setParserConfig(const ParseConfig& config);

		// Performance monitoring
		PerformanceMetrics getMetrics() const { return _metrics; }
		void			   resetMetrics();
		std::string		   getPerformanceReport() const;
		void			   enableMetrics(bool enable = true) { _enableMetrics = enable; }
		bool			   isMetricsEnabled() const { return _enableMetrics; }

		// Performance testing
		void runPerformanceTest(const std::vector<std::pair<int32_t, int32_t>>& testChunks);

	  private:
		// Performance tracking helpers
		void   recordQuery(double loadTime, bool cacheHit) const;
		double getCurrentTime() const;
	};
}; // namespace World
