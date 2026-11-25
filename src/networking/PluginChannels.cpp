#include "network/PluginChannels.hpp"

#include "logger.hpp"
#include "network/PacketIds.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"

#include <map>
#include <mutex>

namespace PluginChannels {
	namespace {
		std::mutex&						   handlersMutex() {
			static std::mutex mutex;
			return mutex;
		}
		std::map<std::string, Handler>& handlers() {
			static std::map<std::string, Handler> map;
			return map;
		}

		// minecraft:register / unregister: channel names separated by NUL bytes
		std::vector<std::string> channelList(Buffer& data) {
			std::vector<std::string> names;
			std::string				 name;
			while (data.remaining() > 0) {
				char c = static_cast<char>(data.readByte());
				if (c == '\0') {
					if (!name.empty()) names.push_back(name);
					name.clear();
				} else if (name.size() < 32767) {
					name += c;
				}
			}
			if (!name.empty()) names.push_back(name);
			return names;
		}
	} // namespace

	void registerHandler(const std::string& channel, Handler handler) {
		std::lock_guard lock(handlersMutex());
		handlers()[channel] = std::move(handler);
	}

	void handle(Server& server, Player& player, Buffer& data) {
		std::string channel = data.readString(32767);
		if (channel == "minecraft:brand") {
			player.setClientBrand(data.readString(32767));
			return;
		}
		if (channel == "minecraft:register" || channel == "minecraft:unregister") {
			std::vector<std::string> names = channelList(data);
			PluginState&			 state = player.pluginState();
			std::lock_guard			 lock(state.mutex);
			for (const std::string& name : names) {
				if (channel == "minecraft:register") {
					if (state.channels.size() < 128) state.channels.insert(name); // Vanilla-like cap against floods
				} else {
					state.channels.erase(name);
				}
			}
			return;
		}
		Handler handler;
		{
			std::lock_guard lock(handlersMutex());
			auto			it = handlers().find(channel);
			if (it != handlers().end()) handler = it->second;
		}
		if (handler) handler(server, player, data); // Unknown channels: ignored, like vanilla
	}

	void send(Server& server, Player& player, const std::string& channel, const std::vector<uint8_t>& payload) {
		int id;
		switch (player.getPlayerState()) {
		case PlayerState::Configuration: id = PacketId::Configuration::Clientbound::CUSTOM_PAYLOAD; break;
		case PlayerState::Play: id = PacketId::Play::Clientbound::CUSTOM_PAYLOAD; break;
		default: return;
		}
		Buffer data;
		data.writeString(channel);
		data.writeBytes(payload);
		Packet::send(player.shared_from_this(), id, data, server);
	}

	void sendBrand(Server& server, Player& player) {
		Buffer brand;
		brand.writeString(SERVER_BRAND);
		send(server, player, "minecraft:brand", brand.getData());
	}

	bool listensTo(Player& player, const std::string& channel) {
		PluginState&	state = player.pluginState();
		std::lock_guard lock(state.mutex);
		return state.channels.count(channel) > 0;
	}
} // namespace PluginChannels

namespace Cookies {
	bool store(Server& server, Player& player, const std::string& key, const std::vector<uint8_t>& value) {
		if (value.size() > MAX_SIZE) return false;
		int id;
		switch (player.getPlayerState()) {
		case PlayerState::Configuration: id = PacketId::Configuration::Clientbound::STORE_COOKIE; break;
		case PlayerState::Play: id = PacketId::Play::Clientbound::STORE_COOKIE; break;
		default: return false;
		}
		Buffer data;
		data.writeString(key);
		data.writeVarInt(static_cast<int32_t>(value.size()));
		data.writeBytes(value);
		Packet::send(player.shared_from_this(), id, data, server);
		return true;
	}

	bool request(Server& server, Player& player, const std::string& key, Callback callback) {
		int id;
		switch (player.getPlayerState()) {
		case PlayerState::Login: id = PacketId::Login::Clientbound::COOKIE_REQUEST; break;
		case PlayerState::Configuration: id = PacketId::Configuration::Clientbound::COOKIE_REQUEST; break;
		case PlayerState::Play: id = PacketId::Play::Clientbound::COOKIE_REQUEST; break;
		default: return false;
		}
		{
			PluginState&	state = player.pluginState();
			std::lock_guard lock(state.mutex);
			state.cookieRequests[key].push_back(std::move(callback));
		}
		Buffer data;
		data.writeString(key);
		Packet::send(player.shared_from_this(), id, data, server);
		return true;
	}

	void handleResponse(Player& player, Buffer& data) {
		std::string							key = data.readString(32767);
		std::optional<std::vector<uint8_t>> value;
		if (data.readBool()) {
			int size = data.readVarInt();
			if (size < 0 || static_cast<size_t>(size) > MAX_SIZE) throw std::runtime_error("Cookie too big");
			value = data.readBytes(static_cast<size_t>(size));
		}
		std::vector<Callback> callbacks;
		{
			PluginState&	state = player.pluginState();
			std::lock_guard lock(state.mutex);
			auto			it = state.cookieRequests.find(key);
			if (it == state.cookieRequests.end() || it->second.empty()) return; // Never asked: ignored
			callbacks.push_back(std::move(it->second.front()));
			it->second.erase(it->second.begin());
			if (it->second.empty()) state.cookieRequests.erase(it);
		}
		for (Callback& callback : callbacks) callback(value);
	}
} // namespace Cookies
