#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <filesystem>
#include <map>
#include <string>

class Config {
  private:
	std::filesystem::path _execPath;

	// Server Config
	std::string _serverMotd;
	std::string _serverAddress;
	int			_serverPort;
	int			_serverSize;

	int			_viewDistance;
	int			_simulationDistance = 10; // server.simulation-distance (chunks around the players that tick)
	int			_compressionThreshold;
	float		_tickRate; // Ticks per second, 20 in vanilla
	int			_fillLimit; // Max blocks per /fill, 32768 in vanilla

	// World Config
	std::string _worldName;
	int			_autosaveInterval;
	std::string _gamemode;
	std::string _difficulty;
	// "spawn-limits": per dimension (or "default"), the natural spawning cap of each mob category ("monster": 70...)
	std::map<std::string, std::map<std::string, int>> _spawnLimits;
	int												  _opPermissionLevel = 4;
	bool											  _acceptsTransfers	 = false;
	// The server's resource pack (server.properties' resource-pack, resource-pack-sha1, require-resource-pack,
	// resource-pack-prompt)
	std::string _resourcePack, _resourcePackSha1, _resourcePackPrompt;
	bool		_requireResourcePack = false;
	bool											  _whiteList = false, _enforceWhitelist = false;

  public:
	Config();
	~Config();
	bool loadConfig();
	bool reloadConfig();

	int			getServerPort();
	int			getServerSize();
	std::string getServerMotd();
	std::string getServerAddress();
	std::string getWorldName();
	std::string getGamemode();
	std::string getDifficulty();
	int			getViewDistance() const { return _viewDistance; }
	int			getSimulationDistance() const { return _simulationDistance; }
	void		setSimulationDistance(int distance) { _simulationDistance = distance; }
	int			getCompressionThreshold() const { return _compressionThreshold; }
	float		getTickRate() const { return _tickRate; }
	int			getAutosaveInterval() const { return _autosaveInterval; }
	int			getFillLimit() const { return _fillLimit; }
	const std::map<std::string, std::map<std::string, int>>& getSpawnLimits() const { return _spawnLimits; }
	int	 getOpPermissionLevel() const { return _opPermissionLevel; }
	bool getWhiteList() const { return _whiteList; }
	bool getAcceptsTransfers() const { return _acceptsTransfers; }
	const std::string& getResourcePack() const { return _resourcePack; }
	const std::string& getResourcePackSha1() const { return _resourcePackSha1; }
	const std::string& getResourcePackPrompt() const { return _resourcePackPrompt; }
	bool			   getRequireResourcePack() const { return _requireResourcePack; }
	bool getEnforceWhitelist() const { return _enforceWhitelist; }
	// /whitelist on|off: kept in config.json ("server": "white-list"), like vanilla's server.properties
	void setWhiteList(bool enabled);
	void setSpawnLimits(std::map<std::string, std::map<std::string, int>> limits) { _spawnLimits = std::move(limits); }

	void setServerSize(int ServerSize);
	void setServerPort(int ServerPort);
	void setServerMotd(std::string ServerMotd);
	void setServerAddress(std::string ServerAddress);
	void setWorldName(std::string WorldName);
	void setGamemode(std::string Gamemode);
	void setDifficulty(std::string Difficulty);
};

#endif
