#ifndef PLUGIN_CHANNELS_HPP
#define PLUGIN_CHANNELS_HPP

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

class Buffer;
class Player;
class Server;

// Custom Payload (plugin channels): the vanilla ones (minecraft:brand both ways, minecraft:register and
// minecraft:unregister for the client's channels) and handlers this server's code registers for its own
namespace PluginChannels {
	// The payload after the channel name
	using Handler = std::function<void(Server&, Player&, Buffer& payload)>;

	// What this server answers to as its brand (the F3 screen: "mc-cpp-server")
	constexpr const char* SERVER_BRAND = "mc-cpp-server";

	void registerHandler(const std::string& channel, Handler handler);
	// A Custom Payload from a client (configuration or play): the channel, then its data
	void handle(Server& server, Player& player, Buffer& data);
	// Sends a payload on a channel, in the player's current state (configuration or play)
	void send(Server& server, Player& player, const std::string& channel, const std::vector<uint8_t>& payload);
	// ServerConfigurationPacketListenerImpl.startConfiguration: the server's brand
	void sendBrand(Server& server, Player& player);
	// Whether its client registered the channel (minecraft:register)
	bool listensTo(Player& player, const std::string& channel);
} // namespace PluginChannels

// Cookies (Store Cookie, Cookie Request / Response): data a client keeps for the server across transfers
namespace Cookies {
	// The answer: the cookie, or nullopt if the client had none
	using Callback = std::function<void(std::optional<std::vector<uint8_t>>)>;
	constexpr size_t MAX_SIZE = 5120; // ClientboundStoreCookiePacket.MAX_PAYLOAD_SIZE

	// Configuration or play only. False if it is too big or the player isn't in one of those states
	bool store(Server& server, Player& player, const std::string& key, const std::vector<uint8_t>& value);
	// Login, configuration or play: the callback runs when the client answers (on the thread handling it)
	bool request(Server& server, Player& player, const std::string& key, Callback callback);
	// A Cookie Response (any state): the key, then an optional byte array
	void handleResponse(Player& player, Buffer& data);
} // namespace Cookies

#endif
