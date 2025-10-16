#include "logger.hpp"
#include "world/chunkParser.hpp"
#include "world/query.hpp"

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace World {

	OptimizedQuery::OptimizedQuery(Manager& manager) : _worldManager(manager) {
		// Initialize parser with default configuration
		DirectChunkParser::ParseConfig config;
		config.worldHeight		 = 384;
		config.minY				 = -64;
		config.loadLighting		 = true;
		config.optimizePalettes	 = true;
		config.validateData		 = true;
		config.loadBlockEntities = true;
		config.cacheSize		 = 64;

		_parser = std::make_unique<DirectChunkParser>(config);

		g_logger->logGameInfo(INFO, "Initialized OptimizedQuery with enhanced chunk parser", "World::OptimizedQuery");
	}

	ChunkData OptimizedQuery::fetchChunkDirect(int32_t chunkX, int32_t chunkZ) {
		auto startTime = getCurrentTime();

		try {
			// Use the world manager's method to locate region file with proper path resolution
			std::filesystem::path regionPath = _worldManager.locateRegionFileByChunkCoord(chunkX, chunkZ);

			// Parse chunk directly from region
			ChunkData chunk = _parser->parseChunkFromRegion(regionPath, chunkX, chunkZ);

			g_logger->logGameInfo(DEBUG,
								  "Successfully loaded chunk (" + std::to_string(chunkX) + ", " + std::to_string(chunkZ) + ") directly",
								  "World::OptimizedQuery");

			// Record successful query
			double loadTime = getCurrentTime() - startTime;
			recordQuery(loadTime, false);

			return chunk;

		} catch (const std::exception& e) {
			g_logger->logGameInfo(ERROR,
								  "Failed to fetch chunk (" + std::to_string(chunkX) + ", " + std::to_string(chunkZ) + "): " + e.what(),
								  "World::OptimizedQuery");

			// Return empty chunk on error
			ChunkData emptyChunk = ChunkData::generateEmpty(chunkX, chunkZ, _parser->getConfig().worldHeight);

			// Record failed query
			double loadTime = getCurrentTime() - startTime;
			recordQuery(loadTime, false);

			return emptyChunk;
		}
	}

	std::shared_ptr<ChunkData> OptimizedQuery::fetchChunkCached(int32_t chunkX, int32_t chunkZ) {
		// Generate cache key
		uint64_t cacheKey = (static_cast<uint64_t>(chunkX) << 32) | static_cast<uint64_t>(chunkZ);

		// Check cache first
		auto it = _chunkCache.find(cacheKey);
		if (it != _chunkCache.end()) {
			g_logger->logGameInfo(
					DEBUG, "Cache hit for chunk (" + std::to_string(chunkX) + ", " + std::to_string(chunkZ) + ")", "World::OptimizedQuery");
			return it->second;
		}

		// Cache miss - load chunk
		try {
			ChunkData chunk = fetchChunkDirect(chunkX, chunkZ);

			// Create shared pointer and cache it
			auto chunkPtr = std::make_shared<ChunkData>(std::move(chunk));

			// Cache management - remove oldest entries if cache is full
			const size_t MAX_CACHE_SIZE = _parser->getConfig().cacheSize;
			if (_chunkCache.size() >= MAX_CACHE_SIZE) {
				// Simple eviction - remove first entry (oldest in insertion order for std::unordered_map)
				auto oldest = _chunkCache.begin();
				g_logger->logGameInfo(DEBUG, "Evicting cached chunk from memory", "World::OptimizedQuery");
				_chunkCache.erase(oldest);
			}

			_chunkCache[cacheKey] = chunkPtr;

			g_logger->logGameInfo(DEBUG,
								  "Cached chunk (" + std::to_string(chunkX) + ", " + std::to_string(chunkZ) +
										  "), cache size: " + std::to_string(_chunkCache.size()),
								  "World::OptimizedQuery");

			return chunkPtr;

		} catch (const std::exception& e) {
			g_logger->logGameInfo(ERROR,
								  "Failed to cache chunk (" + std::to_string(chunkX) + ", " + std::to_string(chunkZ) + "): " + e.what(),
								  "World::OptimizedQuery");

			// Return empty chunk as shared pointer
			auto emptyChunk = std::make_shared<ChunkData>(ChunkData::generateEmpty(chunkX, chunkZ, _parser->getConfig().worldHeight));
			return emptyChunk;
		}
	}

	std::vector<ChunkData> OptimizedQuery::fetchChunkBatch(const std::vector<std::pair<int32_t, int32_t>>& coordinates) {
		auto				   batchStartTime = getCurrentTime();
		std::vector<ChunkData> chunks;
		chunks.reserve(coordinates.size());

		// Record batch query
		if (_enableMetrics) {
			_metrics.batchQueries++;
		}

		// Group chunks by region for efficient batch loading
		std::map<std::pair<int32_t, int32_t>, std::vector<std::pair<int32_t, int32_t>>> regionGroups;

		for (const auto& [chunkX, chunkZ] : coordinates) {
			int32_t regionX = chunkX >> 5;
			int32_t regionZ = chunkZ >> 5;
			regionGroups[{regionX, regionZ}].push_back({chunkX, chunkZ});
		}

		g_logger->logGameInfo(INFO,
							  "Batch loading " + std::to_string(coordinates.size()) + " chunks from " + std::to_string(regionGroups.size()) +
									  " regions",
							  "World::OptimizedQuery");

		// Load chunks region by region
		for (const auto& [regionCoords, chunkCoords] : regionGroups) {
			auto [regionX, regionZ] = regionCoords;

			try {
				// Use the first chunk coordinates to locate the region file
				auto [firstChunkX, firstChunkZ]	 = chunkCoords[0];
				std::filesystem::path regionPath = _worldManager.locateRegionFileByChunkCoord(firstChunkX, firstChunkZ);

				// Use batch parsing if available, otherwise fall back to individual parsing
				std::vector<ChunkData> regionChunks = _parser->parseChunkBatch(regionPath, chunkCoords);
				chunks.insert(chunks.end(), std::make_move_iterator(regionChunks.begin()), std::make_move_iterator(regionChunks.end()));

			} catch (const std::exception& e) {
				// If region file doesn't exist or parsing fails, generate empty chunks
				g_logger->logGameInfo(ERROR, "Failed to batch load region for chunks: " + std::string(e.what()), "World::OptimizedQuery");

				for (const auto& [chunkX, chunkZ] : chunkCoords) {
					chunks.push_back(ChunkData::generateEmpty(chunkX, chunkZ, _parser->getConfig().worldHeight));
				}
			}
		}

		double batchTime = getCurrentTime() - batchStartTime;
		g_logger->logGameInfo(INFO,
							  "Successfully loaded " + std::to_string(chunks.size()) +
									  " chunks in batch, time: " + std::to_string(batchTime * 1000.0) + "ms",
							  "World::OptimizedQuery");

		return chunks;
	}

	void World::OptimizedQuery::clearCache() {
		size_t cacheSize = _chunkCache.size();
		_chunkCache.clear();

		g_logger->logGameInfo(INFO, "Cleared chunk cache (" + std::to_string(cacheSize) + " chunks removed)", "World::OptimizedQuery");
	}

	// Additional utility methods for cache management
	size_t World::OptimizedQuery::getCacheSize() const { return _chunkCache.size(); }

	bool World::OptimizedQuery::isChunkCached(int32_t chunkX, int32_t chunkZ) const {
		uint64_t cacheKey = (static_cast<uint64_t>(chunkX) << 32) | static_cast<uint64_t>(chunkZ);
		return _chunkCache.find(cacheKey) != _chunkCache.end();
	}

	void World::OptimizedQuery::preloadChunks(const std::vector<std::pair<int32_t, int32_t>>& coordinates) {
		g_logger->logGameInfo(INFO, "Preloading " + std::to_string(coordinates.size()) + " chunks into cache", "World::OptimizedQuery");

		auto startTime = getCurrentTime();

		for (const auto& [chunkX, chunkZ] : coordinates) {
			fetchChunkCached(chunkX, chunkZ);
		}

		double preloadTime = getCurrentTime() - startTime;
		g_logger->logGameInfo(INFO,
							  "Preloading completed, cache size: " + std::to_string(_chunkCache.size()) +
									  ", time: " + std::to_string(preloadTime * 1000.0) + "ms",
							  "World::OptimizedQuery");
	}

	void World::OptimizedQuery::evictChunk(int32_t chunkX, int32_t chunkZ) {
		uint64_t cacheKey = (static_cast<uint64_t>(chunkX) << 32) | static_cast<uint64_t>(chunkZ);
		auto	 it		  = _chunkCache.find(cacheKey);

		if (it != _chunkCache.end()) {
			_chunkCache.erase(it);
			g_logger->logGameInfo(
					DEBUG, "Evicted chunk (" + std::to_string(chunkX) + ", " + std::to_string(chunkZ) + ") from cache", "World::OptimizedQuery");
		}
	}

	World::OptimizedQuery::ParseConfig World::OptimizedQuery::getParserConfig() const {
		auto		config = _parser->getConfig();
		ParseConfig result;
		result.worldHeight		 = config.worldHeight;
		result.minY				 = config.minY;
		result.loadLighting		 = config.loadLighting;
		result.optimizePalettes	 = config.optimizePalettes;
		result.validateData		 = config.validateData;
		result.loadBlockEntities = config.loadBlockEntities;
		result.useMemoryMapping	 = config.useMemoryMapping;
		result.cacheSize		 = config.cacheSize;
		return result;
	}

	void World::OptimizedQuery::setParserConfig(const ParseConfig& config) {
		DirectChunkParser::ParseConfig parserConfig;
		parserConfig.worldHeight	   = config.worldHeight;
		parserConfig.minY			   = config.minY;
		parserConfig.loadLighting	   = config.loadLighting;
		parserConfig.optimizePalettes  = config.optimizePalettes;
		parserConfig.validateData	   = config.validateData;
		parserConfig.loadBlockEntities = config.loadBlockEntities;
		parserConfig.useMemoryMapping  = config.useMemoryMapping;
		parserConfig.cacheSize		   = config.cacheSize;

		_parser->setConfig(parserConfig);

		// Clear cache if cache size changed
		if (_chunkCache.size() > config.cacheSize) {
			clearCache();
		}

		g_logger->logGameInfo(INFO, "Updated parser configuration", "World::OptimizedQuery");
	}

	// Performance monitoring methods
	void World::OptimizedQuery::resetMetrics() {
		_metrics = PerformanceMetrics{};
		g_logger->logGameInfo(INFO, "Performance metrics reset", "World::OptimizedQuery");
	}

	std::string World::OptimizedQuery::getPerformanceReport() const {
		std::stringstream report;

		report << "=== OptimizedQuery Performance Report ===\n";
		report << "Total Queries: " << _metrics.totalQueries << "\n";
		report << "Cache Hits: " << _metrics.cacheHits << " (" << std::fixed << std::setprecision(1)
			   << ((_metrics.totalQueries > 0) ? (100.0 * _metrics.cacheHits / _metrics.totalQueries) : 0.0) << "%)\n";
		report << "Cache Misses: " << _metrics.cacheMisses << "\n";
		report << "Batch Queries: " << _metrics.batchQueries << "\n";
		report << "Total Load Time: " << std::fixed << std::setprecision(3) << _metrics.totalLoadTime << "s\n";
		report << "Average Load Time: " << std::fixed << std::setprecision(3) << _metrics.averageLoadTime << "s\n";
		report << "Last Query Time: " << std::fixed << std::setprecision(3) << _metrics.lastQueryTime * 1000.0 << "ms\n";
		report << "Current Cache Size: " << _chunkCache.size() << "\n";
		report << "Metrics Enabled: " << (_enableMetrics ? "Yes" : "No") << "\n";

		return report.str();
	}

	void World::OptimizedQuery::runPerformanceTest(const std::vector<std::pair<int32_t, int32_t>>& testChunks) {
		g_logger->logGameInfo(INFO, "Running performance test on " + std::to_string(testChunks.size()) + " chunks", "World::OptimizedQuery");

		size_t successfulLoads = 0;
		size_t failedLoads	   = 0;
		double totalTime	   = 0.0;
		auto   testStartTime   = getCurrentTime();

		for (const auto& [chunkX, chunkZ] : testChunks) {
			try {
				auto	  startTime = getCurrentTime();
				ChunkData chunk		= fetchChunkDirect(chunkX, chunkZ);
				double	  loadTime	= getCurrentTime() - startTime;

				totalTime += loadTime;
				successfulLoads++;

			} catch (const std::exception& e) {
				failedLoads++;
				g_logger->logGameInfo(ERROR,
									  "Performance test error for chunk (" + std::to_string(chunkX) + ", " + std::to_string(chunkZ) +
											  "): " + e.what(),
									  "World::OptimizedQuery");
			}
		}

		double testTotalTime = getCurrentTime() - testStartTime;
		double averageTime	 = (successfulLoads > 0) ? (totalTime / successfulLoads) : 0.0;

		g_logger->logGameInfo(INFO,
							  "Performance test completed: " + std::to_string(successfulLoads) + " successful loads, " + std::to_string(failedLoads) +
									  " failed, average time: " + std::to_string(averageTime * 1000.0) +
									  "ms, total test time: " + std::to_string(testTotalTime * 1000.0) + "ms",
							  "World::OptimizedQuery");
	}

	// Private performance tracking helpers
	void World::OptimizedQuery::recordQuery(double loadTime, bool cacheHit) const {
		if (!_enableMetrics) return;

		_metrics.totalQueries++;
		_metrics.totalLoadTime += loadTime;
		_metrics.averageLoadTime = _metrics.totalLoadTime / _metrics.totalQueries;
		_metrics.lastQueryTime	 = loadTime;

		if (cacheHit) {
			_metrics.cacheHits++;
		} else {
			_metrics.cacheMisses++;
		}

		// Log performance updates every 100 queries
		if (_metrics.totalQueries % 100 == 0) {
			g_logger->logGameInfo(DEBUG,
								  "Performance update: " + std::to_string(_metrics.totalQueries) + " queries, avg " +
										  std::to_string(_metrics.averageLoadTime * 1000.0) +
										  "ms, cache hit rate: " + std::to_string(100.0 * _metrics.cacheHits / _metrics.totalQueries) + "%",
								  "World::OptimizedQuery");
		}
	}

	double World::OptimizedQuery::getCurrentTime() const {
		auto now	  = std::chrono::high_resolution_clock::now();
		auto duration = now.time_since_epoch();
		return std::chrono::duration<double>(duration).count();
	}

} // namespace World
