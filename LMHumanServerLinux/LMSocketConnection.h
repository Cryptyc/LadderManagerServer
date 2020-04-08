#pragma once
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#define SOCKET int

class LMSocketServer;

struct BotChecksums
{
	std::string BotName;
	std::string Checksum;
	std::string DataChecksum;
	inline bool operator==(const BotChecksums& Other) const
	{
		if (BotName == Other.BotName && Checksum == Other.Checksum)
		{
			return true;
		}
		return false;
	}
};

class LMSocketConnection
{
public:
	LMSocketConnection(SOCKET InClientSocket, int InServerPort, LMSocketServer* InSocketServer);

	bool PrepareInitialRequest(std::string UserName, std::string Token, std::string BotName, BotChecksums& BotFound, std::string& ErrorMessage);

	bool GetBot(const std::string& BotName, const std::string& BotChecksum, const std::string& DataChecksum, std::string& FailiureMessage);

	bool DownloadBot(const std::string& BotName, const std::string& Checksum, bool Data);

	bool HandleConnection();

	void RunBot(std::string BotCommandLine, BotConfig CurrentBotConfig);

	bool ParseConfig(std::string ConfigFileLocation, Document& doc);

	std::string getBotCommandLine(const std::string CommandLine, int gamePort, const int startPort, const std::string& opponentID) const;

	void KillBotProcess();

	SOCKET ClientSocket;

	int ServerPort;

private:
	AgentsConfig Agents;
	unsigned long ProcessId;
	std::future<void> ListenServerTread;
	std::future<void> BotRunThread;
	BotChecksums FindChecksum;
	LMSocketServer* SocketServer;


};

void SendClientResponse(SOCKET AcceptSocket, bool Success, const char* Error);
