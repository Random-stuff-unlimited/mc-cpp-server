#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <filesystem>
#include <string>

class Config {
  private:
	std::filesystem::path _execPath;
	std::string			  _gameVersion;
	std::string			  _serverMotd;
	std::string			  _serverAddress;
	int					  _serverPort;
	int					  _protocolVersion;
	int					  _serverSize;

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

	void setProtocolVersion(int ProtoVersion);
	void setServerSize(int ServerSize);
	void setServerPort(int ServerPort);
	void setServerMotd(std::string ServerMotd);
	void setServerVersion(std::string ServerVersion);
	void setServerAddress(std::string ServerAddress);
};

#endif
