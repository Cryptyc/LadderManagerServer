#pragma once

#include <string>
#include <ctime>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <algorithm>


enum BotType
{
	BinaryCpp,
	CommandCenter,
	Python,
	Wine,
	Mono,
	DotNetCore,
	Java,
	NodeJS,
};

enum class ResultType
{
	InitializationError,
	Timeout,
	ProcessingReplay,
	Player1Win,
	Player1Crash,
	Player1TimeOut,
	Player2Win,
	Player2Crash,
	Player2TimeOut,
	Tie,
	Error
};

enum MatchupListType
{
	File,
	URL,
	None
};

enum class ExitCase
{
	Unknown,
	GameEndVictory,
	GameEndDefeat,
	GameEndTie,
	BotStepTimeout,
	BotCrashed,
	GameTimeOver,
	Error
};

enum Race {
	Terran,
	Zerg,
	Protoss,
	Random
};

enum PlayerType {
	Participant = 1,
	Computer = 2,
	Observer = 3
};

enum class ChatChannel {
	All = 0,
	Team = 1
};


struct GameState
{
	bool IsInGame;
	int GameLoop;
	float Score;
	GameState()
		: IsInGame(true)
		, GameLoop(-1)
		, Score(-1)
	{}

};

struct GameResult
{
	ResultType Result;
	float Bot1AvgFrame;
	float Bot2AvgFrame;
	uint32_t GameLoop;
	std::string TimeStamp;
	GameResult()
		: Result(ResultType::InitializationError)
		, Bot1AvgFrame(0)
		, Bot2AvgFrame(0)
		, GameLoop(0)
		, TimeStamp("")
	{}

};

struct BotConfig
{
	BotType Type;
	std::string BotName;
	std::string RootPath;
	std::string FileName;
	std::string CheckSum;
	Race BotRace;
	std::string Args; //Optional arguments
	std::string PlayerId;
	bool Debug;
	bool Enabled;
	bool Skeleton;
	int ELO;
	std::string executeCommand;
	std::string SurrenderPhrase{ "pineapple" };

	BotConfig()
		: Type(BotType::BinaryCpp)
		, BotRace(Race::Random)
		, Debug(false)
		, Enabled(true)
		, Skeleton(false)
		, ELO(0)
	{}

	BotConfig(BotType InType, const std::string& InBotName, Race InBotRace, const std::string& InBotPath, const std::string& InFileName, const std::string& InArgs = "")
		: Type(InType)
		, BotName(InBotName)
		, RootPath(InBotPath)
		, FileName(InFileName)
		, BotRace(InBotRace)
		, Args(InArgs)
		, Debug(false)
		, Skeleton(false)
	{}

	bool operator ==(const BotConfig& Other) const
	{
		return BotName == Other.BotName;
	}
};

struct Matchup
{
	BotConfig Agent1;
	BotConfig Agent2;
	std::string Bot1Id;
	std::string Bot1Checksum;
	std::string Bot1DataChecksum;
	std::string Bot2Id;
	std::string Bot2Checksum;
	std::string Bot2DataChecksum;
	std::string Map;
	Matchup() {}
	Matchup(const BotConfig& InAgent1, const BotConfig& InAgent2, const std::string& InMap)
		: Agent1(InAgent1),
		Agent2(InAgent2),
		Map(InMap)
	{

	}


};

static MatchupListType GetMatchupListTypeFromString(const std::string GeneratorType)
{
	std::string type(GeneratorType);
	std::transform(type.begin(), type.end(), type.begin(), ::tolower);

	if (type == "url")
	{
		return MatchupListType::URL;
	}
	else if (type == "file")
	{
		return MatchupListType::File;
	}
	else
	{
		return MatchupListType::None;
	}
}

static std::string GetExitCaseString(ExitCase ExitCaseIn)
{
	switch (ExitCaseIn)
	{
	case ExitCase::BotCrashed:
		return "BotCrashed";
	case ExitCase::BotStepTimeout:
		return "BotStepTimeout";
	case ExitCase::Error:
		return "Error";
	case ExitCase::GameEndVictory:
		return "GameEndWin";
	case ExitCase::GameEndDefeat:
		return "GameEndLoss";
	case ExitCase::GameEndTie:
		return "GameEndTie";
	case ExitCase::GameTimeOver:
		return "GameTimeOver";
	case ExitCase::Unknown:
		return "Unknown";
	}
	return "Error";
}

static Race GetRaceFromString(const std::string& RaceIn)
{
	std::string race(RaceIn);
	std::transform(race.begin(), race.end(), race.begin(), ::tolower);

	if (race == "terran")
	{
		return Race::Terran;
	}
	else if (race == "protoss")
	{
		return Race::Protoss;
	}
	else if (race == "zerg")
	{
		return Race::Zerg;
	}
	else if (race == "random")
	{
		return Race::Random;
	}

	return Race::Random;
}

static std::string GetRaceString(const Race RaceIn)
{
	switch (RaceIn)
	{
	case Race::Protoss:
		return "Protoss";
	case Race::Random:
		return "Random";
	case Race::Terran:
		return "Terran";
	case Race::Zerg:
		return "Zerg";
	}
	return "Random";
}



static BotType GetTypeFromString(const std::string& TypeIn)
{
	std::string type(TypeIn);
	std::transform(type.begin(), type.end(), type.begin(), ::tolower);
	if (type == "binarycpp")
	{
		return BotType::BinaryCpp;
	}
	else if (type == "commandcenter")
	{
		return BotType::CommandCenter;
	}
	else if (type == "python")
	{
		return BotType::Python;
	}
	else if (type == "wine")
	{
		return BotType::Wine;
	}
	else if (type == "dotnetcore")
	{
		return BotType::DotNetCore;
	}
	else if (type == "mono")
	{
		return BotType::Mono;
	}
	else if (type == "java")
	{
		return BotType::Java;
	}
	else if (type == "nodejs")
	{
		return BotType::NodeJS;
	}
	return BotType::BinaryCpp;
}

static std::string GetResultType(ResultType InResultType)
{
	switch (InResultType)
	{
	case ResultType::InitializationError:
		return "InitializationError";

	case ResultType::Timeout:
		return "Timeout";

	case ResultType::ProcessingReplay:
		return "ProcessingReplay";

	case ResultType::Player1Win:
		return "Player1Win";

	case ResultType::Player1Crash:
		return "Player1Crash";

	case ResultType::Player1TimeOut:
		return "Player1TimeOut";

	case ResultType::Player2Win:
		return "Player2Win";

	case ResultType::Player2Crash:
		return "Player2Crash";

	case ResultType::Player2TimeOut:
		return "Player2TimeOut";

	case ResultType::Tie:
		return "Tie";

	default:
		return "Error";
	}
}

static std::string RemoveMapExtension(const std::string& filename)
{
	size_t lastdot = filename.find_last_of(".");
	if (lastdot == std::string::npos)
	{
		return filename;
	}
	return filename.substr(0, lastdot);
}

static ResultType getEndResultFromProxyResults(const ExitCase resultBot1, const ExitCase resultBot2)
{
	if ((resultBot1 == ExitCase::BotCrashed && resultBot2 == ExitCase::BotCrashed)
		|| (resultBot1 == ExitCase::BotStepTimeout && resultBot2 == ExitCase::BotStepTimeout)
		|| (resultBot1 == ExitCase::Error && resultBot2 == ExitCase::Error))
	{
		// If both bots crashed we assume the bots are not the problem.
		return ResultType::Error;
	}
	if (resultBot1 == ExitCase::BotCrashed)
	{
		return ResultType::Player1Crash;
	}
	if (resultBot2 == ExitCase::BotCrashed)
	{
		return ResultType::Player2Crash;
	}
	if (resultBot1 == ExitCase::GameEndVictory && (resultBot2 == ExitCase::GameEndDefeat || resultBot2 == ExitCase::BotStepTimeout || resultBot2 == ExitCase::Error))
	{
		return  ResultType::Player1Win;
	}
	if (resultBot2 == ExitCase::GameEndVictory && (resultBot1 == ExitCase::GameEndDefeat || resultBot1 == ExitCase::BotStepTimeout || resultBot1 == ExitCase::Error))
	{
		return  ResultType::Player2Win;
	}
	if (resultBot1 == ExitCase::GameEndTie && resultBot2 == ExitCase::GameEndTie)
	{
		return  ResultType::Tie;
	}
	if (resultBot1 == ExitCase::GameTimeOver && resultBot2 == ExitCase::GameTimeOver)
	{
		return  ResultType::Timeout;
	}
	return ResultType::Error;
}


