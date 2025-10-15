#ifndef BUFFER_HPP
#define BUFFER_HPP

#include "../lib/UUID.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

class Buffer {
  private:
	std::vector<uint8_t> _data;
	size_t				 _pos;

  public:
	Buffer();
	explicit Buffer(const std::vector<uint8_t>& data);

	int	 readVarInt();
	void writeVarInt(int value);
	void writeInt(int32_t value);
	void writeIdentifierArray(const std::vector<std::string>& ids);
	void writeUInt(uint32_t value);

	std::string readString(int maxLength);
	std::string readString(); // Read string without max length limit
	void		writeString(const std::string& str);

	// Array reading methods
	std::vector<std::string>			 readStringArray();
	std::vector<int>					 readVarIntArray();
	template <typename T> std::vector<T> readArray(std::function<T()> reader) {
		int count = readVarInt();
		if (count < 0) {
			throw std::runtime_error("Negative array length");
		}
		std::vector<T> result;
		result.reserve(count);

		for (int i = 0; i < count; ++i) {
			result.push_back(reader());
		}

		return result;
	}

	// Boolean reading methods
	bool readBool();

	std::vector<uint8_t>& getData();
	size_t				  remaining() const;
	uint16_t			  readUShort();
	void				  writeUShort(uint16_t value);
	uint64_t			  readUInt64();
	long				  readLong();
	int32_t				  readInt();
	void				  writeLong(long value);
	uint8_t				  readByte();
	void				  writeByte(uint8_t byte);
	void				  writeBytes(const std::string& data);
	void				  writeBytes(const std::vector<uint8_t>& data);
	void				  writeUUID(const UUID& uuid);

	void	writeBool(bool value);
	void	writeNBT(const std::string& nbtData);
	void	writePosition(int32_t x, int32_t y, int32_t z);
	void	writeFloat(float value);
	void	writeDouble(double value);
	void	writeIdentifier(const std::string& id);
	void	writeVarLong(int64_t value);
	int64_t readVarLong();

	// Known Packs packet specific methods
	struct KnownPack {
		std::string nameSpace;
		std::string id;
		std::string version;
	};
	std::vector<KnownPack> readKnownPacks();
	void				   writeKnownPacks(const std::vector<KnownPack>& packs);
};

#endif
