#ifndef SERVER_ACCESS_HPP
#define SERVER_ACCESS_HPP

#include "lib/UUID.hpp"

#include <ctime>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

// Who may join and who may run which commands (vanilla's PlayerList with its UserBanList, IpBanList, ServerOpList and
// UserWhiteList): ops.json, banned-players.json, banned-ips.json and whitelist.json, in vanilla's format, next to
// config.json. They can be edited by hand while the server is stopped: an entry with only a "name" gets its offline
// UUID. Thread safe: logins check it on the network threads, commands change it on the game thread.
//
// Permission levels (Commands.LEVEL_*): 0 everyone, 1 bypasses spawn protection, 2 game master commands (/tp,
// /gamemode...), 3 admin commands (/op, /kick, /ban, /whitelist), 4 owner
class ServerAccess {
  public:
	struct Op {
		UUID		uuid;
		std::string name;
		int			level = 4;
		bool		bypassesPlayerLimit = false;
	};
	// A ban, by player UUID or by IP (BanListEntry). "expires" is "forever" or a date (BanListEntry.DATE_FORMAT,
	// "yyyy-MM-dd HH:mm:ss Z"): past it, the ban is gone (BanListEntry.hasExpired)
	struct Ban {
		std::string target; // The UUID (as text) or the IP
		std::string name;	// The player's name (empty for IP bans)
		std::string created, source, expires = "forever", reason = "Banned by an operator.";

		bool hasExpired(std::time_t now) const;
	};
	struct Listed {
		UUID		uuid;
		std::string name;
	};
	// Why a login is refused: a translation key of the game and its arguments
	struct Refusal {
		std::string				 key;
		std::vector<std::string> args;
		std::string				 expiration; // A temporary ban's end ("multiplayer.disconnect.banned.expiration"), else empty
	};

	// Reads the four files of the directory (missing ones: empty lists). An empty directory: kept in memory only
	void load(const std::filesystem::path& directory);
	// opPermissionLevel: the level /op gives (server.op-permission-level, 4 in vanilla)
	void setOpPermissionLevel(int level) { _opLevel = level; }
	void setWhitelistEnabled(bool enabled);
	bool whitelistEnabled() const;

	// PlayerList.canPlayerLogin: banned (by UUID or IP) or not whitelisted (ops always are)
	std::optional<Refusal> loginRefusal(const UUID& uuid, const std::string& ip) const;

	// The level of an op, 0 for everyone else
	int	 permissionLevel(const UUID& uuid) const;
	bool op(const UUID& uuid, const std::string& name); // false if it already is one
	bool deop(const UUID& uuid);						 // false if it isn't one
	std::vector<Op> ops() const;

	bool					 isBanned(const UUID& uuid) const;
	// expires: "forever", or a date from expiresIn
	bool					 ban(const UUID& uuid, const std::string& name, const std::string& source, const std::string& reason,
								 const std::string& expires = "forever");
	bool					 pardon(const std::string& name); // By name, ignoring case
	bool					 isIpBanned(const std::string& ip) const;
	bool					 banIp(const std::string& ip, const std::string& source, const std::string& reason, const std::string& expires = "forever");
	bool					 pardonIp(const std::string& ip);
	std::vector<Ban>		 bans() const;
	std::vector<Ban>		 ipBans() const;
	std::optional<Ban>		 banOf(const UUID& uuid) const;
	std::optional<Ban>		 ipBanOf(const std::string& ip) const;

	bool				isWhitelisted(const UUID& uuid) const;
	bool				addToWhitelist(const UUID& uuid, const std::string& name);
	bool				removeFromWhitelist(const UUID& uuid);
	std::vector<Listed> whitelist() const;
	// /whitelist reload: the whitelist (and the ops) read again from their files
	void reloadWhitelist();

	// The date `seconds` from now, as banned-players.json writes it
	static std::string expiresIn(long long seconds);
	// A ban duration: a number and a unit, s, m(inutes), h, d or w ("30m", "7d"); nullopt if malformed or not positive
	static std::optional<long long> parseBanDuration(const std::string& text);

	// InetAddresses.isInetAddress for IPv4: four numbers of 0 to 255
	static bool isIpAddress(const std::string& text);

  private:
	mutable std::mutex	  _mutex;
	std::filesystem::path _directory;
	int					  _opLevel			= 4;
	bool				  _whitelistEnabled = false;
	std::vector<Op>		  _ops;
	std::vector<Ban>	  _bans, _ipBans;
	std::vector<Listed>	  _whitelist;

	void loadOps();
	void loadWhitelist();
	void saveOps() const;
	void saveBans() const;
	void saveIpBans() const;
	void saveWhitelist() const;
};

#endif
