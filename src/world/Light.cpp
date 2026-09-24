#include "world/Light.hpp"

#include "data/GameData.hpp"
#include "world/Chunk.hpp"

#include <algorithm>
#include <cstring>

namespace {
	// The 3x3 chunk area is 48x48 blocks; index (y * AREA + z) * WIDTH + x, y relative to the bottom of the world
	constexpr int WIDTH = 48;
	constexpr int AREA	= WIDTH * WIDTH;

	// Working arrays, reused between chunks of the same thread (a few MB, allocated once per I/O thread)
	struct Workspace {
		std::vector<uint8_t> absorption;
		std::vector<uint8_t> skylightDown;
		std::vector<uint8_t> sky;
		std::vector<uint8_t> block;
		std::vector<int32_t> queue;
	};
	thread_local Workspace workspace;

	// Horizontal distance (in blocks, Manhattan) from each column of the area to the center chunk. Light loses at
	// least 1 per block, so a level L can't reach the center from a column at distance >= L: it isn't propagated
	// there, which skips most of the work in the neighbors
	struct CenterDistance {
		uint8_t column[AREA];
		CenterDistance() {
			for (int z = 0; z < WIDTH; z++) {
				for (int x = 0; x < WIDTH; x++) {
					int dx				  = std::max({0, 16 - x, x - 31});
					int dz				  = std::max({0, 16 - z, z - 31});
					column[z * WIDTH + x] = static_cast<uint8_t>(dx + dz);
				}
			}
		}
	};
	const CenterDistance centerDistance;

	// Spreads light from the queued blocks until nothing changes
	void propagate(std::vector<uint8_t>& light, const Workspace& ws, std::vector<int32_t>& queue, int height, bool sky) {
		for (size_t head = 0; head < queue.size(); head++) {
			int		index  = queue[head];
			uint8_t level  = light[index];
			int		y	   = index / AREA;
			int		column = index - y * AREA;
			int		x = column % WIDTH, z = column / WIDTH;

			auto spread = [&](int neighbor, int neighborColumn, bool down) {
				uint8_t next;
				if (sky && down && level == 15 && ws.skylightDown[neighbor]) {
					next = 15; // Open sky keeps going down
				} else {
					int loss = std::max<int>(1, ws.absorption[neighbor]);
					if (level <= loss) return;
					next = static_cast<uint8_t>(level - loss);
				}
				if (next > light[neighbor] && next > centerDistance.column[neighborColumn]) {
					light[neighbor] = next;
					queue.push_back(neighbor);
				}
			};
			if (x > 0) spread(index - 1, column - 1, false);
			if (x < WIDTH - 1) spread(index + 1, column + 1, false);
			if (z > 0) spread(index - WIDTH, column - WIDTH, false);
			if (z < WIDTH - 1) spread(index + WIDTH, column + WIDTH, false);
			if (y > 0) spread(index - AREA, column, true);
			if (y < height - 1) spread(index + AREA, column, false);
		}
		queue.clear();
	}

	// A section light can cross as if it were empty: only air-like blocks, no light source
	bool isClear(const PalettedContainer& blocks, const LightTables& tables) {
		if (!blocks.isSingleValue()) return false;
		uint32_t state = blocks.singleValue();
		return tables.absorption[state] == 0 && tables.skylightDown[state] && tables.emission[state] == 0;
	}

	// Copies a chunk's light properties into the area at (offsetX, offsetZ), for its `height` lowest blocks
	void fill(Workspace& ws, const Chunk* chunk, int offsetX, int offsetZ, int height, const LightTables& tables) {
		if (!chunk) {
			for (int y = 0; y < height; y++) {
				for (int z = 0; z < 16; z++) {
					size_t row = static_cast<size_t>(y) * AREA + (offsetZ + z) * WIDTH + offsetX;
					std::memset(&ws.absorption[row], 15, 16);
					std::memset(&ws.skylightDown[row], 0, 16);
				}
			}
			return;
		}
		std::lock_guard<std::mutex> lock(chunk->mutex());
		const auto&					sections = chunk->sections();
		for (size_t s = 0; s < static_cast<size_t>(height / 16); s++) {
			const PalettedContainer& blocks = sections[s].blocks;
			if (blocks.isSingleValue() && tables.emission[blocks.singleValue()] == 0) {
				// Uniform section: whole rows at once
				uint32_t state = blocks.singleValue();
				for (int y = 0; y < 16; y++) {
					for (int z = 0; z < 16; z++) {
						size_t row = (s * 16 + y) * AREA + (offsetZ + z) * WIDTH + offsetX;
						std::memset(&ws.absorption[row], tables.absorption[state], 16);
						std::memset(&ws.skylightDown[row], tables.skylightDown[state], 16);
					}
				}
				continue;
			}
			// Decode the packed indices directly, through a small table per palette entry
			const std::vector<uint32_t>& palette  = blocks.palette();
			const std::vector<uint64_t>& data	  = blocks.data();
			uint8_t						 bits	  = blocks.bits();
			uint64_t					 mask	  = (1ULL << bits) - 1;
			int							 perLong  = 64 / bits;
			uint8_t						 absorption[256], skylightDown[256], emission[256];
			for (size_t p = 0; p < palette.size(); p++) {
				absorption[p]	= tables.absorption[palette[p]];
				skylightDown[p] = tables.skylightDown[palette[p]];
				emission[p]		= tables.emission[palette[p]];
			}
			bool   direct = palette.empty();
			int	   i	  = 0;
			size_t row	  = 0;
			for (size_t word = 0; word < data.size() && i < 4096; word++) {
				uint64_t bitsLeft = data[word];
				for (int k = 0; k < perLong && i < 4096; k++, i++, bitsLeft >>= bits) {
					if ((i & 15) == 0) row = (s * 16 + (i >> 8)) * AREA + (offsetZ + ((i >> 4) & 15)) * WIDTH + offsetX;
					uint32_t value = static_cast<uint32_t>(bitsLeft & mask);
					size_t	 index = row + (i & 15);
					uint8_t	 light;
					if (direct) {
						ws.absorption[index]   = tables.absorption[value];
						ws.skylightDown[index] = tables.skylightDown[value];
						light				   = tables.emission[value];
					} else {
						ws.absorption[index]   = absorption[value];
						ws.skylightDown[index] = skylightDown[value];
						light				   = emission[value];
					}
					if (light && light > centerDistance.column[index % AREA]) {
						ws.block[index] = light;
						ws.queue.push_back(static_cast<int32_t>(index));
					}
				}
			}
		}
	}

	// The center chunk's light of one section, row by row (16 contiguous blocks)
	SectionLight pack(const std::vector<uint8_t>& light, int section) {
		SectionLight		 out;
		const uint8_t*		 first	 = &light[static_cast<size_t>(section * 16) * AREA + 16 * WIDTH + 16];
		bool				 uniform = true;
		std::vector<uint8_t> nibbles(2048);
		uint8_t*			 outRow = nibbles.data();
		for (int y = 0; y < 16; y++) {
			for (int z = 0; z < 16; z++, outRow += 8) {
				const uint8_t* row = &light[static_cast<size_t>(section * 16 + y) * AREA + (16 + z) * WIDTH + 16];
				uniform			   = uniform && std::memcmp(row, first, 1) == 0 && std::all_of(row, row + 16, [&](uint8_t v) { return v == *first; });
				for (int x = 0; x < 16; x += 2) outRow[x >> 1] = static_cast<uint8_t>(row[x] | (row[x + 1] << 4));
			}
		}
		if (uniform) {
			out.uniform = *first;
		} else {
			out.nibbles = std::move(nibbles);
		}
		return out;
	}
} // namespace

LightTables::LightTables(const GameData& gameData) {
	int count = gameData.getBlockStateCount();
	absorption.resize(count);
	emission.resize(count);
	skylightDown.resize(count);
	for (int state = 0; state < count; state++) {
		const GameData::StateProperties& props = gameData.getStateProperties(state);
		absorption[state]					   = props.lightBlock;
		emission[state]						   = props.lightEmission;
		skylightDown[state]					   = props.propagatesSkylightDown;
	}
}

size_t ChunkLight::memoryUsage() const {
	size_t total = sizeof(*this) + (sky.capacity() + block.capacity()) * sizeof(SectionLight);
	for (const auto& s : sky) total += s.nibbles.capacity();
	for (const auto& s : block) total += s.nibbles.capacity();
	return total;
}

namespace LightEngine {

	ChunkLight compute(const Chunk* const area[9], const LightTables& tables) {
		Workspace& ws			= workspace;
		int		   sectionCount = static_cast<int>(area[4]->sections().size());

		// Above the highest non-empty section of the area, everything is open sky: only compute up to one section
		// higher (block light from the top can climb into it), the rest is uniform
		int computed = 0;
		for (int i = 0; i < 9; i++) {
			if (!area[i]) continue;
			std::lock_guard<std::mutex> lock(area[i]->mutex());
			const auto&					sections = area[i]->sections();
			for (int s = sectionCount - 1; s >= computed; s--) {
				if (!isClear(sections[s].blocks, tables)) {
					computed = s + 1;
					break;
				}
			}
		}
		computed	  = std::min(sectionCount, computed + 1);
		int	   height = computed * 16;
		size_t cells  = static_cast<size_t>(height) * AREA;

		ws.absorption.resize(cells);
		ws.skylightDown.resize(cells);
		ws.sky.assign(cells, 0);
		ws.block.assign(cells, 0);
		ws.queue.clear();

		// Block light sources are queued while filling
		for (int i = 0; i < 9; i++) fill(ws, area[i], (i % 3) * 16, (i / 3) * 16, height, tables);
		std::vector<int32_t> emitters;
		emitters.swap(ws.queue);

		// Sky: 15 from the top of each column down to the first block that stops it
		std::vector<int> top(AREA, height);
		for (int column = 0; column < AREA; column++) {
			if (centerDistance.column[column] >= 15) continue; // Too far to light the center
			int y = height - 1;
			while (y >= 0 && ws.skylightDown[static_cast<size_t>(y) * AREA + column]) {
				ws.sky[static_cast<size_t>(y) * AREA + column] = 15;
				y--;
			}
			top[column] = y + 1; // Lowest block at 15
			if (y >= 0) {
				// The block that stopped it still receives the sky from above, minus its absorption
				size_t index = static_cast<size_t>(y) * AREA + column;
				int	   loss	 = std::max<int>(1, ws.absorption[index]);
				if (loss < 15) {
					ws.sky[index] = static_cast<uint8_t>(15 - loss);
					ws.queue.push_back(static_cast<int32_t>(index));
				}
			}
		}
		// Only the lit blocks next to a darker column can spread light sideways: between the column's own top and
		// the highest top of its neighbors
		for (int column = 0; column < AREA; column++) {
			if (centerDistance.column[column] >= 15) continue;
			int x = column % WIDTH, z = column / WIDTH;
			int highest = top[column] + 1;
			if (x > 0) highest = std::max(highest, top[column - 1]);
			if (x < WIDTH - 1) highest = std::max(highest, top[column + 1]);
			if (z > 0) highest = std::max(highest, top[column - WIDTH]);
			if (z < WIDTH - 1) highest = std::max(highest, top[column + WIDTH]);
			for (int y = top[column]; y < std::min(highest, height); y++) ws.queue.push_back(y * AREA + column);
		}
		propagate(ws.sky, ws, ws.queue, height, true);

		ws.queue.swap(emitters);
		propagate(ws.block, ws, ws.queue, height, false);

		ChunkLight light;
		light.sky.reserve(sectionCount);
		light.block.reserve(sectionCount);
		for (int s = 0; s < computed; s++) {
			light.sky.push_back(pack(ws.sky, s));
			light.block.push_back(pack(ws.block, s));
		}
		SectionLight openSky, dark;
		openSky.uniform = 15;
		for (int s = computed; s < sectionCount; s++) {
			light.sky.push_back(openSky);
			light.block.push_back(dark);
		}
		return light;
	}

	// ----- Incremental update -----

	namespace {
		// Light of a 3x3 chunk window, addressed with window coordinates (x, z in 0-47, y from the bottom)
		class Window {
		  public:
			Window(Chunk* const chunks[9], const LightTables& tables, Changes& changes) : _chunks(chunks), _tables(tables), _changes(changes) {
				_height = static_cast<int>(chunks[4]->sections().size()) * 16;
				for (int i = 0; i < 9; i++) {
					_changes.sky[i].assign(_height / 16, false);
					_changes.block[i].assign(_height / 16, false);
				}
			}

			int	 height() const { return _height; }
			bool contains(int x, int y, int z) const { return x >= 0 && x < WIDTH && z >= 0 && z < WIDTH && y >= 0 && y < _height; }

			uint8_t get(bool sky, int x, int y, int z) const {
				const ChunkLight& light = _chunks[chunkIndex(x, z)]->light();
				return (sky ? light.sky : light.block)[y >> 4].get(index(x, y, z));
			}

			void set(bool sky, int x, int y, int z, uint8_t level) {
				int			  chunk	  = chunkIndex(x, z);
				ChunkLight&	  light	  = _chunks[chunk]->light();
				SectionLight& section = (sky ? light.sky : light.block)[y >> 4];
				int			  i		  = index(x, y, z);
				if (section.get(i) == level) return;
				if (section.isUniform()) section.nibbles.assign(2048, static_cast<uint8_t>(section.uniform * 0x11));
				uint8_t& byte = section.nibbles[i >> 1];
				byte		  = (i & 1) ? static_cast<uint8_t>((byte & 0x0F) | (level << 4)) : static_cast<uint8_t>((byte & 0xF0) | level);
				(sky ? _changes.sky : _changes.block)[chunk][y >> 4] = true;
			}

			uint32_t state(int x, int y, int z) const {
				const Chunk* chunk = _chunks[chunkIndex(x, z)];
				return chunk->getBlock(x & 15, chunk->minY() + y, z & 15);
			}
			uint8_t absorption(int x, int y, int z) const { return _tables.absorption[state(x, y, z)]; }
			bool	skylightDown(int x, int y, int z) const { return _tables.skylightDown[state(x, y, z)]; }
			uint8_t emission(int x, int y, int z) const { return _tables.emission[state(x, y, z)]; }

			// Changed sections that ended up with a single level go back to one byte
			void compact() {
				for (int c = 0; c < 9; c++) {
					for (bool sky : {true, false}) {
						auto& changed = (sky ? _changes.sky : _changes.block)[c];
						auto& layer	  = sky ? _chunks[c]->light().sky : _chunks[c]->light().block;
						for (size_t s = 0; s < changed.size(); s++) {
							SectionLight& section = layer[s];
							if (!changed[s] || section.isUniform()) continue;
							uint8_t first = section.nibbles[0];
							if ((first >> 4) == (first & 15) && std::all_of(section.nibbles.begin(), section.nibbles.end(), [&](uint8_t b) { return b == first; })) {
								section.uniform = first & 15;
								section.nibbles.clear();
								section.nibbles.shrink_to_fit();
							}
						}
					}
				}
			}

		  private:
			Chunk* const*	   _chunks;
			const LightTables& _tables;
			Changes&		   _changes;
			int				   _height;

			static int chunkIndex(int x, int z) { return (z / 16) * 3 + x / 16; }
			static int index(int x, int y, int z) { return ((y & 15) << 8) | ((z & 15) << 4) | (x & 15); }
		};

		struct Node {
			int16_t x, y, z;
			uint8_t level;
		};
		constexpr int DIRECTIONS[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}, {0, 1, 0}, {0, -1, 0}}; // Last one: down

		void updateLayer(Window& window, bool sky, int x, int y, int z) {
			std::vector<Node> removal;
			std::vector<Node> addition;

			// 1. Remove the light of the changed block and, in cascade, the light that came through it
			uint8_t old = window.get(sky, x, y, z);
			window.set(sky, x, y, z, 0);
			if (old > 0) removal.push_back({static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(z), old});
			for (size_t head = 0; head < removal.size(); head++) {
				Node node = removal[head];
				for (int d = 0; d < 6; d++) {
					int nx = node.x + DIRECTIONS[d][0], ny = node.y + DIRECTIONS[d][1], nz = node.z + DIRECTIONS[d][2];
					if (!window.contains(nx, ny, nz)) continue;
					uint8_t level = window.get(sky, nx, ny, nz);
					if (level == 0) continue;
					// Lit through the removed light: dimmer, or the open sky column going straight down
					bool fromIt = level < node.level || (sky && d == 5 && node.level == 15 && level == 15);
					if (fromIt) {
						window.set(sky, nx, ny, nz, 0);
						removal.push_back({static_cast<int16_t>(nx), static_cast<int16_t>(ny), static_cast<int16_t>(nz), level});
					} else {
						// Lit by something else: it will spread again into the dark area
						addition.push_back({static_cast<int16_t>(nx), static_cast<int16_t>(ny), static_cast<int16_t>(nz), level});
					}
				}
			}

			// 2. The block itself (new light source), and its lit neighbors, which spread into it again
			if (!sky) {
				uint8_t emission = window.emission(x, y, z);
				if (emission > 0) {
					window.set(false, x, y, z, emission);
					addition.push_back({static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(z), emission});
				}
			} else if (y == window.height() - 1 && window.skylightDown(x, y, z)) {
				window.set(true, x, y, z, 15); // Top of the world: open sky
				addition.push_back({static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(z), 15});
			}
			for (int d = 0; d < 6; d++) {
				int nx = x + DIRECTIONS[d][0], ny = y + DIRECTIONS[d][1], nz = z + DIRECTIONS[d][2];
				if (!window.contains(nx, ny, nz)) continue;
				uint8_t level = window.get(sky, nx, ny, nz);
				if (level > 0) addition.push_back({static_cast<int16_t>(nx), static_cast<int16_t>(ny), static_cast<int16_t>(nz), level});
			}

			// 3. Spread, same rules as the full computation
			for (size_t head = 0; head < addition.size(); head++) {
				Node	node  = addition[head];
				uint8_t level = window.get(sky, node.x, node.y, node.z);
				if (level == 0) continue;
				for (int d = 0; d < 6; d++) {
					int nx = node.x + DIRECTIONS[d][0], ny = node.y + DIRECTIONS[d][1], nz = node.z + DIRECTIONS[d][2];
					if (!window.contains(nx, ny, nz)) continue;
					uint8_t next;
					if (sky && d == 5 && level == 15 && window.skylightDown(nx, ny, nz)) {
						next = 15;
					} else {
						int loss = std::max<int>(1, window.absorption(nx, ny, nz));
						if (level <= loss) continue;
						next = static_cast<uint8_t>(level - loss);
					}
					if (next > window.get(sky, nx, ny, nz)) {
						window.set(sky, nx, ny, nz, next);
						addition.push_back({static_cast<int16_t>(nx), static_cast<int16_t>(ny), static_cast<int16_t>(nz), next});
					}
				}
			}
		}
	} // namespace

	Changes update(Chunk* const window[9], int x, int y, int z, const LightTables& tables) {
		Changes changes;
		Window	area(window, tables, changes);
		updateLayer(area, true, 16 + x, y, 16 + z);
		updateLayer(area, false, 16 + x, y, 16 + z);
		area.compact();
		return changes;
	}

} // namespace LightEngine
