#ifndef UUID_HPP
#define UUID_HPP

#include <cstdint>
#include <string>

class Buffer;
class Player;

class UUID {
  private:
	uint64_t _mostSigBits;
	uint64_t _leastSigBits;

  public:
	UUID();
	UUID(uint64_t most, uint64_t least);
	~UUID();

	uint64_t getMostSigBits() const;
	uint64_t getLeastSigBits() const;
	void	 setMostSigBits(uint64_t val);
	void	 setLeastSigBits(uint64_t val);

	std::string toString() const;

	void		readFromBuffer(Buffer& buf);
	void		writeToBuffer(Buffer& buf) const;
	static UUID fromOfflinePlayer(const std::string& name);
};

#endif
