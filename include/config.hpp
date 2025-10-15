#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <filesystem>
#include <string>

class Config {
  private:
	std::filesystem::path _execPath;

	// Version Config
	std::string _gameVersion;
	int			_protocolVersion;

	// Server Config
	std::string _serverMotd;
	std::string _serverAddress;
	int			_serverPort;
	int			_serverSize;

	// World Config
	std::string _worldName;
	std::string _gamemode;
	std::string _difficulty;

  public:
	Config();
	~Config();
	bool loadConfig();
	bool reloadConfig();

	int			getServerPort();
	int			getProtocolVersion();
	int			getServerSize();
	std::string getVersion();
	std::string getServerMotd();
	std::string getServerAddress();
	std::string getWorldName();
	std::string getGamemode();
	std::string getDifficulty();

	void setProtocolVersion(int ProtoVersion);
	void setServerSize(int ServerSize);
	void setServerPort(int ServerPort);
	void setServerMotd(std::string ServerMotd);
	void setServerVersion(std::string ServerVersion);
	void setServerAddress(std::string ServerAddress);
	void setWorldName(std::string WorldName);
	void setGamemode(std::string Gamemode);
	void setDifficulty(std::string Difficulty);
};

#endif
