#pragma once
using namespace rapidjson;
#include "AgentsConfig.h"
#include <map>
#include <vector>
#include <thread>
#include <future>

#define SOCKET int

class LMSocketConnection;

struct BotChecksumData
{
	std::string BotName;
	std::string Checksum;
	std::string DataChecksum;
	inline bool operator==(const BotChecksumData& Other) const
	{
		if (BotName == Other.BotName && Checksum == Other.Checksum)
		{
			return true;
		}
		return false;
	}
};

struct LMSocketForward
{
	SOCKET ClientSocket;
	std::thread* ClientThread;
	LMSocketConnection* SocketConnection;
	bool bIsAvialable = false;
	LMSocketForward(SOCKET InClientSocket, std::thread* InClientThread, LMSocketConnection* InSocketConnection)
		: ClientSocket(InClientSocket)
		, ClientThread(InClientThread)
		, SocketConnection(InSocketConnection)
	{}
};

class LMSocketServer
{
public:
	LMSocketServer();

	int RunServer();
	LMSocketConnection* GetSocketForPort(int Port);
	void SetPortAvailble(int Port);
	bool IsBotKnown(std::string BotName, std::string BotChecksum);
	void AddKnownBot(std::string BotName, std::string BotChecksum, std::string DataChecksum);

private:
	std::map<int, LMSocketForward*> PortMap;
	std::vector<std::thread*> BotThreads;
	std::vector<BotChecksumData> KnownChecksums;

};

void ListenForBot(LMSocketServer* Server, int *Port);
