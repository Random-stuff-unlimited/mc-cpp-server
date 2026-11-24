#include "ServerAccess.hpp"

#include "lib/json.hpp"
#include "logger.hpp"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <fstream>

namespace {
	using json = nlohmann::json;

	// "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" (UUID.fromString), nullopt if malformed
	std::optional<UUID> parseUuid(const std::string& text) {
		std::string hex;
		for (char c : text) {
			if (c == '-') continue;
			if (!std::isxdigit(static_cast<unsigned char>(c))) return std::nullopt;
			hex += c;
		}
		if (hex.size() != 32) return std::nullopt;
		return UUID(std::stoull(hex.substr(0, 16), nullptr, 16), std::stoull(hex.substr(16), nullptr, 16));
	}

	// An entry's UUID: its "uuid", else the offline UUID of its "name" (an entry written by hand)
	std::optional<UUID> entryUuid(const json& entry) {
		if (entry.contains("uuid") && entry["uuid"].is_string()) {
			if (auto uuid = parseUuid(entry["uuid"].get<std::string>())) return uuid;
		}
		if (entry.contains("name") && entry["name"].is_string()) return UUID::fromOfflinePlayer(entry["name"].get<std::string>());
		return std::nullopt;
	}

	json readList(const std::filesystem::path& file) {
		std::ifstream in(file);
		if (!in) return json::array();
		try {
			json list = json::parse(in);
			if (list.is_array()) return list;
		} catch (const std::exception& e) {
			g_logger->logGameInfo(ERROR, "Couldn't read " + file.string() + ": " + e.what(), "SERVER");
		}
		return json::array();
	}

	void writeList(const std::filesystem::path& file, const json& list) {
		std::ofstream out(file);
		if (!out) {
			g_logger->logGameInfo(ERROR, "Couldn't write " + file.string(), "SERVER");
			return;
		}
		out << list.dump(2) << "\n";
	}

	// BanListEntry.DATE_FORMAT: "yyyy-MM-dd HH:mm:ss Z"
	std::string now() {
		std::time_t t = std::time(nullptr);
		std::tm		tm{};
		gmtime_r(&t, &tm);
		char buf[32];
		std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S +0000", &tm);
		return buf;
	}

	std::string lower(std::string text) {
		std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return std::tolower(c); });
		return text;
	}

	ServerAccess::Ban readBan(const json& entry, const std::string& target) {
		ServerAccess::Ban ban;
		ban.target	= target;
		ban.name	= entry.value("name", "");
		ban.created = entry.value("created", "");
		ban.source	= entry.value("source", "(Unknown)");
		ban.expires = entry.value("expires", "forever");
		ban.reason	= entry.value("reason", "Banned by an operator.");
		return ban;
	}

	json writeBan(const ServerAccess::Ban& ban, const char* targetKey) {
		json entry = {{targetKey, ban.target}};
		if (!ban.name.empty()) entry["name"] = ban.name;
		entry["created"] = ban.created;
		entry["source"]	 = ban.source;
		entry["expires"] = ban.expires;
		entry["reason"]	 = ban.reason;
		return entry;
	}
} // namespace

void ServerAccess::load(const std::filesystem::path& directory) {
	std::lock_guard lock(_mutex);
	_directory = directory;
	_bans.clear();
	_ipBans.clear();
	if (_directory.empty()) {
		_ops.clear();
		_whitelist.clear();
		return;
	}
	loadOps();
	loadWhitelist();
	for (const json& entry : readList(_directory / "banned-players.json")) {
		if (auto uuid = entryUuid(entry)) _bans.push_back(readBan(entry, uuid->toString()));
	}
	for (const json& entry : readList(_directory / "banned-ips.json")) {
		if (entry.contains("ip") && entry["ip"].is_string()) _ipBans.push_back(readBan(entry, entry["ip"].get<std::string>()));
	}
}

void ServerAccess::loadOps() {
	_ops.clear();
	for (const json& entry : readList(_directory / "ops.json")) {
		auto uuid = entryUuid(entry);
		if (!uuid) continue;
		_ops.push_back({*uuid, entry.value("name", ""), std::clamp(entry.value("level", _opLevel), 0, 4), entry.value("bypassesPlayerLimit", false)});
	}
}

void ServerAccess::loadWhitelist() {
	_whitelist.clear();
	for (const json& entry : readList(_directory / "whitelist.json")) {
		if (auto uuid = entryUuid(entry)) _whitelist.push_back({*uuid, entry.value("name", "")});
	}
}

void ServerAccess::saveOps() const {
	if (_directory.empty()) return;
	json list = json::array();
	for (const Op& op : _ops) {
		list.push_back({{"uuid", op.uuid.toString()}, {"name", op.name}, {"level", op.level}, {"bypassesPlayerLimit", op.bypassesPlayerLimit}});
	}
	writeList(_directory / "ops.json", list);
}

void ServerAccess::saveBans() const {
	if (_directory.empty()) return;
	json list = json::array();
	for (const Ban& ban : _bans) list.push_back(writeBan(ban, "uuid"));
	writeList(_directory / "banned-players.json", list);
}

void ServerAccess::saveIpBans() const {
	if (_directory.empty()) return;
	json list = json::array();
	for (const Ban& ban : _ipBans) list.push_back(writeBan(ban, "ip"));
	writeList(_directory / "banned-ips.json", list);
}

void ServerAccess::saveWhitelist() const {
	if (_directory.empty()) return;
	json list = json::array();
	for (const Listed& entry : _whitelist) list.push_back({{"uuid", entry.uuid.toString()}, {"name", entry.name}});
	writeList(_directory / "whitelist.json", list);
}

void ServerAccess::setWhitelistEnabled(bool enabled) {
	std::lock_guard lock(_mutex);
	_whitelistEnabled = enabled;
}

bool ServerAccess::whitelistEnabled() const {
	std::lock_guard lock(_mutex);
	return _whitelistEnabled;
}

std::optional<ServerAccess::Refusal> ServerAccess::loginRefusal(const UUID& uuid, const std::string& ip) const {
	if (auto ban = banOf(uuid)) return Refusal{"multiplayer.disconnect.banned.reason", {ban->reason}};
	if (!ip.empty()) {
		if (auto ban = ipBanOf(ip)) return Refusal{"multiplayer.disconnect.banned_ip.reason", {ban->reason}};
	}
	if (whitelistEnabled() && permissionLevel(uuid) == 0 && !isWhitelisted(uuid)) return Refusal{"multiplayer.disconnect.not_whitelisted", {}};
	return std::nullopt;
}

int ServerAccess::permissionLevel(const UUID& uuid) const {
	std::lock_guard lock(_mutex);
	for (const Op& op : _ops) {
		if (op.uuid == uuid) return op.level;
	}
	return 0;
}

bool ServerAccess::op(const UUID& uuid, const std::string& name) {
	std::lock_guard lock(_mutex);
	if (std::any_of(_ops.begin(), _ops.end(), [&](const Op& op) { return op.uuid == uuid; })) return false;
	_ops.push_back({uuid, name, _opLevel, false});
	saveOps();
	return true;
}

bool ServerAccess::deop(const UUID& uuid) {
	std::lock_guard lock(_mutex);
	auto			it = std::find_if(_ops.begin(), _ops.end(), [&](const Op& op) { return op.uuid == uuid; });
	if (it == _ops.end()) return false;
	_ops.erase(it);
	saveOps();
	return true;
}

std::vector<ServerAccess::Op> ServerAccess::ops() const {
	std::lock_guard lock(_mutex);
	return _ops;
}

bool ServerAccess::isBanned(const UUID& uuid) const { return banOf(uuid).has_value(); }

std::optional<ServerAccess::Ban> ServerAccess::banOf(const UUID& uuid) const {
	std::lock_guard lock(_mutex);
	std::string		target = uuid.toString();
	for (const Ban& ban : _bans) {
		if (ban.target == target) return ban;
	}
	return std::nullopt;
}

bool ServerAccess::ban(const UUID& uuid, const std::string& name, const std::string& source, const std::string& reason) {
	if (isBanned(uuid)) return false;
	std::lock_guard lock(_mutex);
	_bans.push_back({uuid.toString(), name, now(), source, "forever", reason});
	saveBans();
	return true;
}

bool ServerAccess::pardon(const std::string& name) {
	std::lock_guard lock(_mutex);
	auto it = std::find_if(_bans.begin(), _bans.end(), [&](const Ban& ban) { return lower(ban.name) == lower(name); });
	if (it == _bans.end()) return false;
	_bans.erase(it);
	saveBans();
	return true;
}

bool ServerAccess::isIpBanned(const std::string& ip) const { return ipBanOf(ip).has_value(); }

std::optional<ServerAccess::Ban> ServerAccess::ipBanOf(const std::string& ip) const {
	std::lock_guard lock(_mutex);
	for (const Ban& ban : _ipBans) {
		if (ban.target == ip) return ban;
	}
	return std::nullopt;
}

bool ServerAccess::banIp(const std::string& ip, const std::string& source, const std::string& reason) {
	if (isIpBanned(ip)) return false;
	std::lock_guard lock(_mutex);
	_ipBans.push_back({ip, "", now(), source, "forever", reason});
	saveIpBans();
	return true;
}

bool ServerAccess::pardonIp(const std::string& ip) {
	std::lock_guard lock(_mutex);
	auto			it = std::find_if(_ipBans.begin(), _ipBans.end(), [&](const Ban& ban) { return ban.target == ip; });
	if (it == _ipBans.end()) return false;
	_ipBans.erase(it);
	saveIpBans();
	return true;
}

std::vector<ServerAccess::Ban> ServerAccess::bans() const {
	std::lock_guard lock(_mutex);
	return _bans;
}

std::vector<ServerAccess::Ban> ServerAccess::ipBans() const {
	std::lock_guard lock(_mutex);
	return _ipBans;
}

bool ServerAccess::isWhitelisted(const UUID& uuid) const {
	std::lock_guard lock(_mutex);
	return std::any_of(_whitelist.begin(), _whitelist.end(), [&](const Listed& entry) { return entry.uuid == uuid; });
}

bool ServerAccess::addToWhitelist(const UUID& uuid, const std::string& name) {
	if (isWhitelisted(uuid)) return false;
	std::lock_guard lock(_mutex);
	_whitelist.push_back({uuid, name});
	saveWhitelist();
	return true;
}

bool ServerAccess::removeFromWhitelist(const UUID& uuid) {
	std::lock_guard lock(_mutex);
	auto it = std::find_if(_whitelist.begin(), _whitelist.end(), [&](const Listed& entry) { return entry.uuid == uuid; });
	if (it == _whitelist.end()) return false;
	_whitelist.erase(it);
	saveWhitelist();
	return true;
}

std::vector<ServerAccess::Listed> ServerAccess::whitelist() const {
	std::lock_guard lock(_mutex);
	return _whitelist;
}

void ServerAccess::reloadWhitelist() {
	std::lock_guard lock(_mutex);
	if (_directory.empty()) return;
	loadOps();
	loadWhitelist();
}

bool ServerAccess::isIpAddress(const std::string& text) {
	int	   parts = 0;
	size_t start = 0;
	while (true) {
		size_t end	 = text.find('.', start);
		auto   piece = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
		if (piece.empty() || piece.size() > 3 || !std::all_of(piece.begin(), piece.end(), ::isdigit) || std::stoi(piece) > 255) return false;
		parts++;
		if (end == std::string::npos) break;
		start = end + 1;
	}
	return parts == 4;
}
