#pragma once
using namespace rapidjson;
#include "AgentsConfig.h"
#include <map>


class LMSocketConnection
{
public:
	LMSocketConnection(SOCKET InClientSocket, int InServerPort);

	bool HandleConnection();

	void RunBot(std::string BotCommandLine, BotConfig CurrentBotConfig);

	bool ParseConfig(std::string ConfigFileLocation, Document& doc);

	std::string getBotCommandLine(const std::string CommandLine, int gamePort, const int startPort, const std::string& opponentID) const;

	int ServerPort;
	SOCKET ClientSocket;

private:
	AgentsConfig Agents;

	std::future<void> ListenServerTread;
	std::future<void> BotRunThread;

};

class LMSocketServer
{
public:
	LMSocketServer();

	int RunServer();
private:
	std::map<int, LMSocketConnection*> PortMap;

};
