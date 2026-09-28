#ifndef NBT_WRITER_HPP
#define NBT_WRITER_HPP

#include "lib/nbt.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nbt {

	// Binary NBT writer (big-endian), the counterpart of Parser. Tag ids are the variant's index in Tag.
	// A list takes the type of its first element (TAG_END when empty, like vanilla's ListTag). A list mixing types
	// is written like vanilla 1.21.5+: every element as a compound, the non-compound ones wrapped as {"": value}
	class Writer {
	  public:
		// NbtIo.write: a named root compound (files; the name is empty in vanilla's)
		static std::vector<uint8_t> write(const TagCompound& root, const std::string& name = "");
		// Network NBT: the tag type then its payload, without a name. An empty Tag is TAG_END (no tag)
		static void writeNetwork(const Tag& tag, std::vector<uint8_t>& out);
		static void writePayload(const Tag& tag, std::vector<uint8_t>& out);
		static uint8_t typeOf(const Tag& tag) { return static_cast<uint8_t>(tag.data.index()); }
	};

	// Unwraps an element of a mixed list (a compound with only the key ""), as ListTag does when reading
	const Tag& unwrapListElement(const Tag& tag);

} // namespace nbt

#endif
