#include "UUID.hpp"
#include "buffer.hpp"
#include <sstream>
#include <iomanip>
#include <string>
#include <cstdint>
#include <iostream>

UUID::UUID() : _mostSigBits(0), _leastSigBits(0) {}
UUID::UUID(uint64_t most, uint64_t least) : _mostSigBits(most), _leastSigBits(least) {}
UUID::~UUID() {}

uint64_t UUID::getMostSigBits() const { return  _mostSigBits; }
uint64_t UUID::getLeastSigBits() const { return _leastSigBits; }
void UUID::setMostSigBits(uint64_t val) { _mostSigBits = val; }
void UUID::setLeastSigBits(uint64_t val) { _leastSigBits = val; }

std::string UUID::toString() const {
    std::stringstream ss;
    ss << std::hex << std::setfill('0')
       << std::setw(8) << ((_mostSigBits >> 32) & 0xFFFFFFFF) << "-"
       << std::setw(4) << ((_mostSigBits >> 16) & 0xFFFF) << "-"
       << std::setw(4) << (_mostSigBits & 0xFFFF) << "-"
       << std::setw(4) << ((_leastSigBits >> 48) & 0xFFFF) << "-"
       << std::setw(12) << (_leastSigBits & 0xFFFFFFFFFFFFULL);
    return ss.str();
}

void UUID::readFromBuffer(Buffer &buf) {
    uint64_t most = 0, least = 0;
    for (int i = 0; i < 8; ++i) most = (most << 8) | buf.readByte();
    for (int i = 0; i < 8; ++i) least = (least << 8) | buf.readByte();
    _mostSigBits = most;
    _leastSigBits = least;
}

void UUID::writeToBuffer(Buffer &buf) const {
    for (int i = 7; i >= 0; --i) buf.writeByte((_mostSigBits >> (i*8)) & 0xFF);
    for (int i = 7; i >= 0; --i) buf.writeByte((_leastSigBits >> (i*8)) & 0xFF);
}
