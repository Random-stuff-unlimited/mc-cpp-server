#include "lib/nbtParser.hpp"

#include <bit>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
	constexpr uint8_t TAG_END		 = 0;
	constexpr uint8_t TAG_BYTE		 = 1;
	constexpr uint8_t TAG_SHORT		 = 2;
	constexpr uint8_t TAG_INT		 = 3;
	constexpr uint8_t TAG_LONG		 = 4;
	constexpr uint8_t TAG_FLOAT		 = 5;
	constexpr uint8_t TAG_DOUBLE	 = 6;
	constexpr uint8_t TAG_BYTE_ARRAY = 7;
	constexpr uint8_t TAG_STRING	 = 8;
	constexpr uint8_t TAG_LIST		 = 9;
	constexpr uint8_t TAG_COMPOUND	 = 10;
	constexpr uint8_t TAG_INT_ARRAY	 = 11;
	constexpr uint8_t TAG_LONG_ARRAY = 12;
} // namespace

namespace nbt {

	NBT Parser::parse(const std::vector<uint8_t>& data) {
		if (data.empty()) {
			throw std::runtime_error("NBT parsing error: empty data.");
		}

		size_t cursor = 0;

		uint8_t rootType = read<uint8_t>(data, cursor);
		if (rootType != TAG_COMPOUND) {
			throw std::runtime_error("NBT parsing error: root must be a TAG_Compound.");
		}

		std::string rootName = parseTagName(data, cursor);
		TagCompound rootComp = parseCompound(data, cursor);

		return NBT(rootName, rootComp);
	}

	template <typename T> T Parser::read(const std::vector<uint8_t>& data, size_t& cursor) {
		if (cursor + sizeof(T) > data.size()) {
			throw std::runtime_error("NBT parsing error: unexpected end of data.");
		}
		T value;
		std::memcpy(&value, &data[cursor], sizeof(T));
		cursor += sizeof(T);

		if constexpr (sizeof(T) > 1) {
			if (std::endian::native == std::endian::little) {
				return std::byteswap(value);
			}
		}
		return value;
	}

	// Explicit instantiations for the types we use
	template int8_t	  Parser::read<int8_t>(const std::vector<uint8_t>&, size_t&);
	template uint8_t  Parser::read<uint8_t>(const std::vector<uint8_t>&, size_t&);
	template int16_t  Parser::read<int16_t>(const std::vector<uint8_t>&, size_t&);
	template uint16_t Parser::read<uint16_t>(const std::vector<uint8_t>&, size_t&);
	template int32_t  Parser::read<int32_t>(const std::vector<uint8_t>&, size_t&);
	template uint32_t Parser::read<uint32_t>(const std::vector<uint8_t>&, size_t&);
	template int64_t  Parser::read<int64_t>(const std::vector<uint8_t>&, size_t&);
	template uint64_t Parser::read<uint64_t>(const std::vector<uint8_t>&, size_t&);

	std::string Parser::parseTagName(const std::vector<uint8_t>& data, size_t& cursor) {
		uint16_t length = read<uint16_t>(data, cursor);
		if (length == 0) {
			return "";
		}
		if (cursor + length > data.size()) {
			throw std::runtime_error("NBT parsing error: invalid tag name length.");
		}
		std::string name(data.begin() + cursor, data.begin() + cursor + length);
		cursor += length;
		return name;
	}

	TagCompound Parser::parseCompound(const std::vector<uint8_t>& data, size_t& cursor) {
		TagCompound compound;
		while (cursor < data.size()) {
			uint8_t tagType = read<uint8_t>(data, cursor);
			if (tagType == TAG_END) {
				break;
			}

			std::string tagName = parseTagName(data, cursor);
			compound[tagName]	= parseTag(data, cursor, tagType);
		}
		return compound;
	}

	TagList Parser::parseList(const std::vector<uint8_t>& data, size_t& cursor) {
		TagList list;
		uint8_t listType = read<uint8_t>(data, cursor);
		int32_t length	 = read<int32_t>(data, cursor);

		if (length < 0) {
			throw std::runtime_error("NBT parsing error: negative TAG_List length.");
		}

		for (int32_t i = 0; i < length; ++i) {
			list.push_back(parseTag(data, cursor, listType));
		}
		return list;
	}

	Tag Parser::parseTag(const std::vector<uint8_t>& data, size_t& cursor, uint8_t tagType) {
		switch (tagType) {
		case TAG_BYTE:
			return Tag(read<TagByte>(data, cursor));
		case TAG_SHORT:
			return Tag(read<TagShort>(data, cursor));
		case TAG_INT:
			return Tag(read<TagInt>(data, cursor));
		case TAG_LONG:
			return Tag(read<TagLong>(data, cursor));
		case TAG_FLOAT:
			return Tag(std::bit_cast<TagFloat>(read<uint32_t>(data, cursor)));
		case TAG_DOUBLE:
			return Tag(std::bit_cast<TagDouble>(read<uint64_t>(data, cursor)));
		case TAG_STRING: {
			uint16_t length = read<uint16_t>(data, cursor);
			if (cursor + length > data.size()) {
				throw std::runtime_error("NBT parsing error: invalid string length.");
			}
			std::string str(data.begin() + cursor, data.begin() + cursor + length);
			cursor += length;
			return Tag(str);
		}
		case TAG_BYTE_ARRAY: {
			int32_t length = read<int32_t>(data, cursor);
			if (length < 0) {
				throw std::runtime_error("NBT parsing error: negative TAG_Byte_Array length.");
			}
			TagByteArray array;
			if (length > 0) {
				array.resize(static_cast<size_t>(length));
				std::memcpy(array.data(), &data[cursor], static_cast<size_t>(length));
				cursor += static_cast<size_t>(length);
			}
			return Tag(array);
		}
		case TAG_INT_ARRAY: {
			int32_t length = read<int32_t>(data, cursor);
			if (length < 0) {
				throw std::runtime_error("NBT parsing error: negative TAG_Int_Array length.");
			}
			TagIntArray array;
			array.reserve(static_cast<size_t>(length));
			for (int32_t i = 0; i < length; ++i) {
				array.push_back(read<TagInt>(data, cursor));
			}
			return Tag(array);
		}
		case TAG_LONG_ARRAY: {
			int32_t length = read<int32_t>(data, cursor);
			if (length < 0) {
				throw std::runtime_error("NBT parsing error: negative TAG_Long_Array length.");
			}
			TagLongArray array;
			array.reserve(static_cast<size_t>(length));
			for (int32_t i = 0; i < length; ++i) {
				array.push_back(read<TagLong>(data, cursor));
			}
			return Tag(array);
		}
		case TAG_LIST:
			return Tag(std::make_shared<TagList>(parseList(data, cursor)));
		case TAG_COMPOUND:
			return Tag(std::make_shared<TagCompound>(parseCompound(data, cursor)));
		default:
			throw std::runtime_error("NBT parsing error: unsupported tag type: " + std::to_string(tagType));
		}
	}

} // namespace nbt
