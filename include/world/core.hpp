#ifndef CORE_HPP
#define CORE_HPP

#include <cstdint>
// ============================================================================
// CORE DATA TYPES
// ============================================================================

enum class ChunkState : uint8_t {
	UNLOADED = 0, // Not in memory
	LOADING,	  // Background I/O in progress
	CACHED,		  // In cache but no active in simulation
	ACTIVATING,	  // Being conveted to LiveChunk
	ACTIVE,		  // Active in simulation
	DIRTY,		  // Active and modified
	SAVING,		  // Being saved to disk
	EVICTING	  // Being removed from memory
};

#endif
