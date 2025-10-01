#include "UUID.hpp"
#include "buffer.hpp"
#include <sstream>
#include <iomanip>

UUID::UUID() : mostSigBits(0), leastSigBits(0) {}
UUID::UUID(uint64_t most, uint64_t least) : mostSigBits(most), leastSigBits(least) {}
UUID::~UUID() {}

uint64_t UUID::getMostSigBits() const { return mostSigBits; }
uint64_t UUID::getLeastSigBits() const { return leastSigBits; }
void UUID::setMostSigBits(uint64_t val) { mostSigBits = val; }
void UUID::setLeastSigBits(uint64_t val) { leastSigBits = val; }

std::string UUID::toString() const {
    std::stringstream ss;
    ss << std::hex << std::setfill('0')
       << std::setw(8) << ((mostSigBits >> 32) & 0xFFFFFFFF) << "-"
       << std::setw(4) << ((mostSigBits >> 16) & 0xFFFF) << "-"
       << std::setw(4) << (mostSigBits & 0xFFFF) << "-"
       << std::setw(4) << ((leastSigBits >> 48) & 0xFFFF) << "-"
       << std::setw(12) << (leastSigBits & 0xFFFFFFFFFFFFULL);
    return ss.str();
}

void UUID::readFromBuffer(Buffer &buf) {
    uint64_t most = 0, least = 0;
    for (int i = 0; i < 8; ++i) most = (most << 8) | buf.readByte();
    for (int i = 0; i < 8; ++i) least = (least << 8) | buf.readByte();
    mostSigBits = most;
    leastSigBits = least;
}

void UUID::writeToBuffer(Buffer &buf) const {
    for (int i = 7; i >= 0; --i) buf.writeByte((mostSigBits >> (i*8)) & 0xFF);
    for (int i = 7; i >= 0; --i) buf.writeByte((leastSigBits >> (i*8)) & 0xFF);
}
