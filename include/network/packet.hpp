#ifndef PACKET_HPP
#define PACKET_HPP

#include "../lib/UUID.hpp"
#include "../player.hpp"
#include "buffer.hpp"
#include "server.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

enum PacketResult { PACKET_OK = 0, PACKET_SEND = 1, PACKET_DISCONNECT = 2, PACKET_ERROR = -1 };

// Protocol limit: a packet length is a VarInt of at most 3 bytes
constexpr int32_t MAX_PACKET_SIZE = 2097151;

class Packet {
  private:
	int32_t					_size;
	int32_t					_id;
	Buffer					_data;
	std::shared_ptr<Player> _player;
	int						_socketFd;
	int						_returnPacket;

  public:
	// Incoming packet: payload is the frame content after the packet id
	Packet(std::shared_ptr<Player> player, int32_t id, const std::vector<uint8_t>& payload, int32_t size);
	// Outgoing packet with no data yet
	explicit Packet(std::shared_ptr<Player> player);
	Packet(const Packet& other);
	Packet& operator=(const Packet& other);
	~Packet();
	static int varintLen(int value);
	Player*	   getPlayer() const;
	uint32_t   getSize();
	uint32_t   getId();
	Buffer&	   getData();
	int		   getSocket() const;
	void	   setReturnPacket(int value);
	int		   getReturnPacket();
	int		   getVarintSize(int32_t value);
	void	   setPacketSize(int32_t value);
	void	   setPacketId(uint32_t value);
	// Frames id + data and queues it for the sender thread; this packet is left untouched
	void sendPacket(int id, Buffer& data, Server& server);

	// Same, for code that has a player but no incoming packet
	static void send(const std::shared_ptr<Player>& player, int id, Buffer& data, Server& server);
	// Queues an already framed packet (see buildFrame)
	static void sendFrame(const std::shared_ptr<Player>& player, std::vector<uint8_t> frame, Server& server);

	// Length-prefixed packet, in the compressed format when compressionThreshold >= 0:
	// payloads of at least compressionThreshold bytes are zlib-compressed
	static std::vector<uint8_t> buildFrame(int id, const std::vector<uint8_t>& data, int compressionThreshold, int compressionLevel = 1);
};

#endif
