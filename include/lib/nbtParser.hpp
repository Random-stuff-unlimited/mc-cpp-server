#ifndef NBT_PARSER_HPP
#define NBT_PARSER_HPP

#include "lib/nbt.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nbt {

	// NBT binary parser for Minecraft's Named Binary Tag format.
	// This class parses a binary NBT buffer into the simplified NBT model in lib/nbt.hpp.
	class Parser {
	  public:
		Parser() = default;

		// Parse an NBT payload (binary) into an NBT object.
		// Expects the data to start with a root tag type and name.
		NBT parse(const std::vector<uint8_t>& data);
		// Network NBT (anonymous root, 1.20.2+): a tag type then its payload, from data at cursor. TAG_END alone
		// (no tag) gives an empty Tag
		Tag parseNetwork(const std::vector<uint8_t>& data, size_t& cursor);

	  private:
		// Helper utilities for decoding the binary stream (big-endian).
		template <typename T> T read(const std::vector<uint8_t>& data, size_t& cursor);
		std::string				parseTagName(const std::vector<uint8_t>& data, size_t& cursor);
		TagCompound				parseCompound(const std::vector<uint8_t>& data, size_t& cursor);
		TagList					parseList(const std::vector<uint8_t>& data, size_t& cursor);
		Tag						parseTag(const std::vector<uint8_t>& data, size_t& cursor, uint8_t tagType);
	};

} // namespace nbt

#endif // NBT_PARSER_HPP
