#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <filesystem>
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
	int			_compressionThreshold;
	float		_tickRate; // Ticks per second, 20 in vanilla

	// World Config
	std::string _worldName;
	int			_autosaveInterval;
	std::string _gamemode;
	std::string _difficulty;

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
	int			getCompressionThreshold() const { return _compressionThreshold; }
	float		getTickRate() const { return _tickRate; }
	int			getAutosaveInterval() const { return _autosaveInterval; }

	void setServerSize(int ServerSize);
	void setServerPort(int ServerPort);
	void setServerMotd(std::string ServerMotd);
	void setServerAddress(std::string ServerAddress);
	void setWorldName(std::string WorldName);
	void setGamemode(std::string Gamemode);
	void setDifficulty(std::string Difficulty);
};

#endif
