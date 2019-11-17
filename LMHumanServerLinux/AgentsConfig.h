#pragma once

#include <string>
#include <map>
#include "Types.h"

#define PLAYER_ID_LENGTH 16

class AgentsConfig
{
public:
	void LoadAgents(const std::string& BaseDirectory, const std::string& BotConfigFile);
	void SaveBotConfig(const BotConfig& Agent);

	bool FindBot(const std::string& BotName, BotConfig& ReturnBot);

	std::map<std::string, BotConfig> BotConfigs;



};
#pragma once
