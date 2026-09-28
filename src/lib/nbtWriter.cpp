#include "lib/nbtWriter.hpp"

#include <bit>
#include <stdexcept>

namespace {
	constexpr uint8_t TAG_END	   = 0;
	constexpr uint8_t TAG_COMPOUND = 10;

	template <typename T> void put(std::vector<uint8_t>& out, T value) {
		auto bits = std::bit_cast<std::make_unsigned_t<T>>(value);
		for (int shift = (sizeof(T) - 1) * 8; shift >= 0; shift -= 8) out.push_back(static_cast<uint8_t>(bits >> shift));
	}

	void putFloat(std::vector<uint8_t>& out, float value) { put(out, std::bit_cast<int32_t>(value)); }
	void putDouble(std::vector<uint8_t>& out, double value) { put(out, std::bit_cast<int64_t>(value)); }

	// Modified UTF-8 differs from UTF-8 only for NUL and characters outside the BMP, which names and texts of the
	// game don't use: written as is
	void putString(std::vector<uint8_t>& out, const std::string& value) {
		if (value.size() > 65535) throw std::runtime_error("NBT string too long");
		put(out, static_cast<uint16_t>(value.size()));
		out.insert(out.end(), value.begin(), value.end());
	}

	void putLength(std::vector<uint8_t>& out, size_t size) { put(out, static_cast<int32_t>(size)); }
} // namespace

namespace nbt {

	std::vector<uint8_t> Writer::write(const TagCompound& root, const std::string& name) {
		std::vector<uint8_t> out;
		out.push_back(TAG_COMPOUND);
		putString(out, name);
		for (const auto& [key, value] : root) {
			out.push_back(typeOf(value));
			putString(out, key);
			writePayload(value, out);
		}
		out.push_back(TAG_END);
		return out;
	}

	void Writer::writeNetwork(const Tag& tag, std::vector<uint8_t>& out) {
		out.push_back(typeOf(tag));
		if (typeOf(tag) != TAG_END) writePayload(tag, out);
	}

	void Writer::writePayload(const Tag& tag, std::vector<uint8_t>& out) {
		std::visit(
				[&out](const auto& value) {
					using T = std::decay_t<decltype(value)>;
					if constexpr (std::is_same_v<T, TagEnd>) {
					} else if constexpr (std::is_same_v<T, TagByte> || std::is_same_v<T, TagShort> || std::is_same_v<T, TagInt> ||
										 std::is_same_v<T, TagLong>) {
						put(out, value);
					} else if constexpr (std::is_same_v<T, TagFloat>) {
						putFloat(out, value);
					} else if constexpr (std::is_same_v<T, TagDouble>) {
						putDouble(out, value);
					} else if constexpr (std::is_same_v<T, TagString>) {
						putString(out, value);
					} else if constexpr (std::is_same_v<T, TagByteArray> || std::is_same_v<T, TagIntArray> || std::is_same_v<T, TagLongArray>) {
						putLength(out, value.size());
						for (auto element : value) put(out, element);
					} else if constexpr (std::is_same_v<T, std::shared_ptr<TagCompound>>) {
						if (value) {
							for (const auto& [key, child] : *value) {
								out.push_back(typeOf(child));
								putString(out, key);
								writePayload(child, out);
							}
						}
						out.push_back(TAG_END);
					} else if constexpr (std::is_same_v<T, std::shared_ptr<TagList>>) {
						if (!value || value->data.empty()) {
							out.push_back(TAG_END);
							putLength(out, 0);
							return;
						}
						uint8_t type  = typeOf(value->data.front());
						bool	mixed = false;
						for (const Tag& element : value->data) mixed |= typeOf(element) != type;
						// ListTag.wrapIfNeeded: a compound whose only key is "" would be read back unwrapped
						auto needsWrap = [](const Tag& element) {
							if (typeOf(element) != TAG_COMPOUND) return true;
							const auto& compound = element.get<std::shared_ptr<TagCompound>>();
							return compound && compound->size() == 1 && compound->contains("");
						};
						out.push_back(mixed ? TAG_COMPOUND : type);
						putLength(out, value->data.size());
						for (const Tag& element : value->data) {
							if (mixed && needsWrap(element)) {
								TagCompound wrapper;
								wrapper[""] = element;
								writePayload(Tag(std::make_shared<TagCompound>(std::move(wrapper))), out);
							} else {
								writePayload(element, out);
							}
						}
					}
				},
				tag.data);
	}

	const Tag& unwrapListElement(const Tag& tag) {
		if (const auto* compound = std::get_if<std::shared_ptr<TagCompound>>(&tag.data)) {
			if (*compound && (*compound)->size() == 1 && (*compound)->contains("")) return (*compound)->at("");
		}
		return tag;
	}

} // namespace nbt
