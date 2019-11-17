#include "AgentsConfig.h"
#define RAPIDJSON_HAS_STDSTRING 1
#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;

void AgentsConfig::LoadAgents(const std::string& BaseDirectory, const std::string& BotConfigFile)
{
	if (BotConfigFile.length() < 1)
	{
		return;
	}
	std::ifstream t(BotConfigFile);
	std::stringstream buffer;
	buffer << t.rdbuf();
	std::string BotConfigString = buffer.str();
	rapidjson::Document doc;
	bool parsingFailed = doc.Parse(BotConfigString.c_str()).HasParseError();
	if (parsingFailed)
	{
		std::cerr << "Unable to parse bot config file: " << BotConfigFile << std::endl;
		return;
	}
	if (doc.HasMember("Bots") && doc["Bots"].IsObject())
	{
		const rapidjson::Value& Bots = doc["Bots"];
		for (auto itr = Bots.MemberBegin(); itr != Bots.MemberEnd(); ++itr)
		{
			BotConfig NewBot;
			NewBot.BotName = itr->name.GetString();
			const rapidjson::Value& val = itr->value;

			if (val.HasMember("Race") && val["Race"].IsString())
			{
				NewBot.BotRace = GetRaceFromString(val["Race"].GetString());
			}
			else
			{
				std::cerr << "Unable to parse race for bot " << NewBot.BotName << std::endl;
				continue;
			}
			if (val.HasMember("Type") && val["Type"].IsString())
			{
				NewBot.Type = GetTypeFromString(val["Type"].GetString());
			}
			else
			{
				std::cerr << "Unable to parse type for bot " << NewBot.BotName << std::endl;
				continue;
			}
			if (val.HasMember("RootPath") && val["RootPath"].IsString())
			{
				NewBot.RootPath = BaseDirectory;
				if (NewBot.RootPath.back() != '/')
				{
					NewBot.RootPath += '/';
				}
				NewBot.RootPath = NewBot.RootPath + val["RootPath"].GetString();
				if (NewBot.RootPath.back() != '/')
				{
					NewBot.RootPath += '/';
				}
			}
			else
			{
				std::cerr << "Unable to parse root path for bot " << NewBot.BotName << std::endl;
				continue;
			}
			if (val.HasMember("FileName") && val["FileName"].IsString())
			{
				NewBot.FileName = val["FileName"].GetString();
			}
			else
			{
				std::cerr << "Unable to parse file name for bot " << NewBot.BotName << std::endl;
				continue;
			}
			if (!fs::exists(NewBot.RootPath + NewBot.FileName))
			{
				std::cerr << "Unable to parse bot " << NewBot.BotName << std::endl;
				std::cerr << "Is the path " << NewBot.RootPath << " correct?" << std::endl;
				continue;
			}
			const std::string dataLocation = NewBot.RootPath + "/data";
			fs::create_directories(dataLocation); // If a directory exists this just fails and does nothing.
			if (val.HasMember("Args") && val["Args"].IsString())
			{
				NewBot.Args = val["Args"].GetString();
			}
			if (val.HasMember("Debug") && val["Debug"].IsBool()) {
				NewBot.Debug = val["Debug"].GetBool();
			}

			if (val.HasMember("SurrenderPhrase") && val["SurrenderPhrase"].IsString()) {
				NewBot.SurrenderPhrase = val["SurrenderPhrase"].GetString();
			}

			std::string OutCmdLine = "";
			switch (NewBot.Type)
			{
			case Python:
			{
				OutCmdLine = "Python " + NewBot.FileName;
				break;
			}
			case Wine:
			{
				OutCmdLine = "wine " + NewBot.FileName;
				break;
			}
			case Mono:
			{
				OutCmdLine = "mono " + NewBot.FileName;
				break;
			}
			case DotNetCore:
			{
				OutCmdLine = "dotnet " + NewBot.FileName;
				break;
			}
			case BinaryCpp:
			{
				OutCmdLine = NewBot.RootPath + NewBot.FileName;
				break;
			}
			case Java:
			{
				OutCmdLine = "java -jar " + NewBot.FileName;
				break;
			}
			}

			if (NewBot.Args != "")
			{
				OutCmdLine += " " + NewBot.Args;
			}

			NewBot.executeCommand = OutCmdLine;
			BotConfig SavedBot;
			if (FindBot(NewBot.BotName, SavedBot))
			{
				NewBot.CheckSum = SavedBot.CheckSum;
				BotConfigs[NewBot.BotName] = NewBot;
			}
			else
			{
				BotConfigs.insert(std::make_pair(std::string(NewBot.BotName), NewBot));
			}

		}
	}

}

void AgentsConfig::SaveBotConfig(const BotConfig& Agent)
{
	BotConfig SavedBot;
	if (FindBot(Agent.BotName, SavedBot))
	{
		BotConfigs[Agent.BotName] = Agent;
	}
	else
	{
		BotConfigs.insert(std::make_pair(std::string(Agent.BotName), Agent));
	}
}


bool AgentsConfig::FindBot(const std::string& BotName, BotConfig& ReturnBot)
{
	auto ThisBot = BotConfigs.find(BotName);
	if (ThisBot != BotConfigs.end())
	{
		ReturnBot = ThisBot->second;
		return true;
	}
	return false;
}