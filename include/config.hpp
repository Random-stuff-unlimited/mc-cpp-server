#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>

class Config {
  private:
	std::string _gameVersion;
	std::string _serverMotd;
	std::string _serverAddress;
	int _serverPort;
	int _protocolVersion;
	int _serverSize;

  public:
	bool loadConfig();
	bool reloadConfig();
};

#endif
