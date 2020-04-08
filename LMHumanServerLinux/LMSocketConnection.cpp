#undef UNICODE

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/un.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>

#define RAPIDJSON_HAS_STDSTRING 1

#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"


#include "LMHumanSockets.h"
#include "LMSocketConnection.h"
#include "Tools.h"

//#define BOT_DIR "/home/crysison/Bots/"
#define BOT_DIR "/home/crypt/Bots/"
#define USERNAME "LadderBox1"
#define PASSWORD "Sc2AiLadder"
#define DEFAULT_BUFLEN 8192

#define REMOTE_CHECK_FILE "http://sc2ai.net/GetBotTest.php"
#define BOT_DOWNLOAD_PATH "http://sc2ai.net/DownloadBot.php"


#define ZeroMemory(Destination,Length) memset((Destination),0,(Length))
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#define INVALID_SOCKET  (SOCKET)(~0)
#define SOCKET_ERROR            (-1)

using namespace rapidjson;


static std::string GetBotStartCommand(BotType Type, std::string FileName, std::string args)
{
	std::string OutCmdLine = "";
	switch (Type)
	{
	case Python:
	{
		OutCmdLine = "python " + FileName;
		break;
	}
	case Wine:
	{
		OutCmdLine = "wine " + FileName;
		break;
	}
	case Mono:
	{
		OutCmdLine = "mono " + FileName;
		break;
	}
	case DotNetCore:
	{
		OutCmdLine = "dotnet " + FileName;
		break;
	}
	case BinaryCpp:
	{
		OutCmdLine = "wine " + FileName;
		break;
	}
	case Java:
	{
		OutCmdLine = "java -jar " + FileName;
		break;
	}
	case CommandCenter:
	case NodeJS:
		break;
	}

	OutCmdLine += " " + args;
	return OutCmdLine;
}


LMSocketConnection::LMSocketConnection(SOCKET InClientSocket, int InServerPort, LMSocketServer* InSocketServer)
	: ClientSocket(InClientSocket)
	, ServerPort(InServerPort)
	, SocketServer(InSocketServer)
{

}

void SendClientResponse(SOCKET AcceptSocket, bool Success, const char* Error)
{
	Document ResponseDoc;
	ResponseDoc.SetObject();
	rapidjson::Document::AllocatorType& allocator = ResponseDoc.GetAllocator();
	if (!Success)
	{
		ResponseDoc.AddMember(rapidjson::Value("Error", allocator).Move(), rapidjson::Value(Error, allocator).Move(), allocator);
	}
	else
	{
		ResponseDoc.AddMember(rapidjson::Value("Success", allocator).Move(), rapidjson::Value(Error, allocator).Move(), allocator);
	}
	StringBuffer buffer;
	Writer<StringBuffer> writer(buffer);
	ResponseDoc.Accept(writer);
	send(AcceptSocket, buffer.GetString(), buffer.GetLength(), 0);
}



bool LMSocketConnection::PrepareInitialRequest(std::string UserName, std::string Token, std::string BotName, BotChecksums& BotFound, std::string& ErrorMessage)
{
	std::vector<std::string> arguments;
	std::string argument = " -F Username=" + UserName;
	arguments.push_back(argument);
	argument = " -F Token=" + Token;
	arguments.push_back(argument);
	argument = " -F BotName=" + BotName;
	arguments.push_back(argument);
	std::string ReturnString = PerformRestRequest(REMOTE_CHECK_FILE, arguments);
	Document doc;
	std::string BotChecksum, DataChecksum;
	bool ChecksumFound = false;
	if (doc.Parse(ReturnString).HasParseError())
	{
		ErrorMessage = ReturnString;
		return false;
	}
	if (doc.HasMember("Error") && doc["Error"].IsString())
	{
		ErrorMessage = doc["Error"].GetString();
		return false;
	}
	std::string BotNameDownload;

	if (doc.HasMember("BotName") && doc["BotName"].IsString())
	{
		BotNameDownload = doc["BotName"].GetString();
		if (BotName == BotNameDownload)
		{
			BotFound.BotName = BotNameDownload;
			if (doc.HasMember("BotChecksum") && doc["BotChecksum"].IsString())
			{
				BotFound.Checksum = doc["BotChecksum"].GetString();
			}
			if (doc.HasMember("DataChecksum") && doc["DataChecksum"].IsString())
			{
				BotFound.DataChecksum = doc["DataChecksum"].GetString();
			}
			ChecksumFound = true;
		}
	}
	if (!ChecksumFound)
	{
		ErrorMessage = "Bot not found";
	}
	return ChecksumFound;
}

bool LMSocketConnection::DownloadBot(const std::string& BotName, const std::string& Checksum, bool Data)
{
	constexpr int DownloadRetrys = 3;
	std::string RootPath = std::string(BOT_DIR) + "/" + BotName;
	if (Data)
	{
		RootPath += "/data";
	}
	RemoveDirectoryRecursive(RootPath);
	for (int RetryCount = 0; RetryCount < DownloadRetrys; RetryCount++)
	{
		std::vector<std::string> arguments;
		std::string argument = " -F BotName=" + BotName;
		arguments.push_back(argument);
		argument = std::string(" -F Username=") + USERNAME;
		arguments.push_back(argument);
		argument = std::string(" -F Password=") + PASSWORD;
		arguments.push_back(argument);
		if (Data)
		{
			argument = " -F Data=1";
			arguments.push_back(argument);
		}
		std::string BotZipLocation = BOT_DIR + std::string("/") + BotName + ".zip";
		remove(BotZipLocation.c_str());
		argument = " -o " + BotZipLocation;
		arguments.push_back(argument);
		PerformRestRequest(BOT_DOWNLOAD_PATH, arguments);
		std::string BotMd5 = GenerateMD5(BotZipLocation);
		std::cout << "Download checksum: " << Checksum << " Bot checksum: " << BotMd5 << std::endl;

		if (BotMd5.compare(Checksum) == 0)
		{
			UnzipArchive(BotZipLocation, RootPath);
			remove(BotZipLocation.c_str());
			return true;
		}
		remove(BotZipLocation.c_str());
	}
	return false;
}

bool LMSocketConnection::GetBot(const std::string& BotName, const std::string& BotChecksum, const std::string& DataChecksum, std::string& FailureMessage)
{
	if (!DownloadBot(BotName, BotChecksum, false))
	{
		FailureMessage = "Unable to download bot " + BotName;
		return false;
	}

	if (DataChecksum != "")
	{
		if (!DownloadBot(BotName, DataChecksum, true))
		{
			FailureMessage = "Unable to download data for " + BotName;
			return false;
		}
	}
	else
	{
		const std::string DataLocation = std::string(BOT_DIR) + std::string("/") + BotName + "/data";
		MakeDirectory(DataLocation);
	}
	const std::string BotLocation = std::string(BOT_DIR) + std::string("/") + BotName;

	return true;
}

bool LMSocketConnection::HandleConnection()
{

	// Send and receive data.
	char* recvbuf = (char*)malloc(DEFAULT_BUFLEN);
	if (recvbuf == nullptr)
	{
		printf("unable to allocate memory");
		return false;
	}


	ZeroMemory(recvbuf, DEFAULT_BUFLEN);

	recv(ClientSocket, recvbuf, DEFAULT_BUFLEN, 0);
//	std::cout << "Client said: " << recvbuf << std::endl;
	Document RequestDoc;
	if (RequestDoc.Parse(recvbuf).HasParseError())
	{
		SendClientResponse(ClientSocket, false, "Invalid Json Document\r\n");
		free(recvbuf);
		return false;
	}

	if ((!RequestDoc.HasMember("BotName") || !RequestDoc["BotName"].IsString()))
	{
		SendClientResponse(ClientSocket, false, "Bot Name not found\r\n");
		free(recvbuf);
		return false;
	}

	if ((!RequestDoc.HasMember("StartPort") || !RequestDoc["StartPort"].IsString()))
	{
		SendClientResponse(ClientSocket, false, "Start port not found\r\n");
		free(recvbuf);
		return false;
	}

	if ((!RequestDoc.HasMember("Username") || !RequestDoc["Username"].IsString()))
	{
		SendClientResponse(ClientSocket, false, "Username not found\r\n");
		free(recvbuf);
		return false;
	}
	if ((!RequestDoc.HasMember("Token") || !RequestDoc["Token"].IsString()))
	{
		SendClientResponse(ClientSocket, false, "Token not found\r\n");
		free(recvbuf);
		return false;
	}
	BotChecksums NewBotConfig;
	std::string ErrroMessage;
	if (!PrepareInitialRequest(RequestDoc["Username"].GetString(), RequestDoc["Token"].GetString(), RequestDoc["BotName"].GetString(), NewBotConfig, ErrroMessage))
	{
		SendClientResponse(ClientSocket, false, ErrroMessage.c_str());
		free(recvbuf);
		return false;
	}

	std::string  BotConfigLocation;
	std::string BotRootLocation = BOT_DIR;
	BotRootLocation += RequestDoc["BotName"].GetString();
	BotConfigLocation = BotRootLocation + "/ladderbots.json";
	bool BotAlreadyOnServer = SocketServer->IsBotKnown(NewBotConfig.BotName, NewBotConfig.Checksum) && CheckExists(BotConfigLocation);
	
	if (!BotAlreadyOnServer)
	{
		std::string FailiureMessage;
		if (!GetBot(NewBotConfig.BotName, NewBotConfig.Checksum, NewBotConfig.DataChecksum, FailiureMessage))
		{
			SendClientResponse(ClientSocket, false, FailiureMessage.c_str());
			free(recvbuf);
			return false;

		}
	}

	Agents.LoadAgents(BotRootLocation, BotConfigLocation);
	BotConfig CurrentBotConfig;
	if (!Agents.FindBot(RequestDoc["BotName"].GetString(), CurrentBotConfig))
	{
		SendClientResponse(ClientSocket, false, "Invalid bot config\r\n");
		free(recvbuf);
		return false;
	}
	SocketServer->AddKnownBot(NewBotConfig.BotName, NewBotConfig.Checksum, NewBotConfig.DataChecksum);
	int StartPort = atoi(RequestDoc["StartPort"].GetString());
	SendClientResponse(ClientSocket, true, "Success");
	CurrentBotConfig.executeCommand = GetBotStartCommand(CurrentBotConfig.Type, CurrentBotConfig.FileName, "");
	std::string BotCommandLine = getBotCommandLine(CurrentBotConfig.executeCommand, ServerPort, StartPort, "HUMAN");
	RunBot(BotCommandLine, CurrentBotConfig);

	free(recvbuf);
	return true;
}

void LMSocketConnection::KillBotProcess()
{
	kill(ProcessId, SIGTERM);
}

LMSocketConnection* LMSocketServer::GetSocketForPort(int Port)
{
	if (PortMap[Port] != nullptr)
	{
		return PortMap[Port]->SocketConnection;
	}
	return nullptr;
}
