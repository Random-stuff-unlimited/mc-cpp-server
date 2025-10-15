#ifndef MC_CPP_SERVER_DATA_REGISTRY_UTILS_HPP
#define MC_CPP_SERVER_DATA_REGISTRY_UTILS_HPP

#include "RegistryData.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"

#include <map>
#include <string>
#include <vector>

std::vector<RegistryData> parseMinecraftRegistries();

void sendRegistryData(Packet& packet, Server& server);

bool validateRegistryData(const RegistryData& registry);

RegistryData createRegistryFromMap(const std::string& registryName, const std::map<std::string, uint32_t>& registryData, bool addNamespace = true);

std::vector<uint8_t> serializeRegistryPacket(const RegistryData& registry);

void sendRegistryDataBatch(Packet& packet, Server& server, const std::vector<RegistryData>& registries);

std::vector<RegistryData> createAllEssentialRegistries();

#endif // MC_CPP_SERVER_DATA_REGISTRY_UTILS_HPP
