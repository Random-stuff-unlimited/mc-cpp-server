#include "network/TextComponent.hpp"

#include "network/buffer.hpp"
#include "lib/json.hpp"

namespace {
	constexpr int8_t TAG_END	= 0;
	constexpr int8_t TAG_STRING = 8;
	constexpr int8_t TAG_LIST	= 9;
	constexpr int8_t TAG_COMPOUND = 10;

	// NBT strings: unsigned short length, then the bytes
	void writeNbtString(Buffer& buf, const std::string& value) {
		buf.writeUShort(static_cast<uint16_t>(value.size()));
		buf.writeBytes(value);
	}

	void writeStringField(Buffer& buf, const std::string& name, const std::string& value) {
		buf.writeByte(TAG_STRING);
		writeNbtString(buf, name);
		writeNbtString(buf, value);
	}
	int8_t tagOf(const nlohmann::json& value) {
		if (value.is_object()) return TAG_COMPOUND;
		if (value.is_array()) return TAG_LIST;
		if (value.is_boolean()) return 1;		 // Byte
		if (value.is_number_integer()) return 3; // Int
		if (value.is_number()) return 6;		 // Double
		return TAG_STRING;
	}

	// The payload of a tag (without its type and name)
	void writePayload(Buffer& buf, const nlohmann::json& value) {
		switch (tagOf(value)) {
		case TAG_COMPOUND:
			for (const auto& [key, field] : value.items()) {
				if (field.is_null()) continue;
				buf.writeByte(tagOf(field));
				writeNbtString(buf, key);
				writePayload(buf, field);
			}
			buf.writeByte(TAG_END);
			break;
		case TAG_LIST: {
			// One element type: strings among compounds become {"text": ...} compounds
			bool mixed = false;
			for (const auto& element : value) mixed |= tagOf(element) != tagOf(value.front());
			int8_t type = value.empty() ? TAG_END : mixed ? TAG_COMPOUND : tagOf(value.front());
			buf.writeByte(type);
			buf.writeInt(static_cast<int32_t>(value.size()));
			for (const auto& element : value) {
				if (mixed && !element.is_object()) {
					writePayload(buf, nlohmann::json{{"text", element.is_string() ? element.get<std::string>() : element.dump()}});
				} else {
					writePayload(buf, element);
				}
			}
			break;
		}
		case 1: buf.writeByte(value.get<bool>() ? 1 : 0); break;
		case 3: buf.writeInt(static_cast<int32_t>(value.get<int64_t>())); break;
		case 6: buf.writeDouble(value.get<double>()); break;
		default: writeNbtString(buf, value.is_string() ? value.get<std::string>() : value.dump()); break;
		}
	}
} // namespace

namespace TextComponent {
	bool writeJson(Buffer& buf, const std::string& text) {
		size_t start = text.find_first_not_of(' ');
		if (start == std::string::npos || (text[start] != '{' && text[start] != '[' && text[start] != '"')) {
			writeText(buf, text);
			return true;
		}
		nlohmann::json value = nlohmann::json::parse(text, nullptr, false);
		if (value.is_discarded()) return false;
		if (value.is_array() && value.empty()) value = "";
		buf.writeByte(tagOf(value));
		writePayload(buf, value);
		return true;
	}


	void writeText(Buffer& buf, const std::string& text, const std::string& color) {
		buf.writeByte(TAG_COMPOUND); // Network NBT: no root name
		writeStringField(buf, "text", text);
		if (!color.empty()) writeStringField(buf, "color", color);
		buf.writeByte(TAG_END);
	}

	void writeTranslatable(Buffer& buf, const std::string& key, const std::vector<std::string>& args, const std::string& color) {
		buf.writeByte(TAG_COMPOUND);
		writeStringField(buf, "translate", key);
		if (!args.empty()) {
			// A plain string is a valid component, so the arguments are a list of strings
			buf.writeByte(TAG_LIST);
			writeNbtString(buf, "with");
			buf.writeByte(TAG_STRING);
			buf.writeInt(static_cast<int32_t>(args.size()));
			for (const std::string& arg : args) writeNbtString(buf, arg);
		}
		if (!color.empty()) writeStringField(buf, "color", color);
		buf.writeByte(TAG_END);
	}

} // namespace TextComponent
