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

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/un.h>
#include <errno.h>
#include <netdb.h>

#define RAPIDJSON_HAS_STDSTRING 1

#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include "LMHumanSockets.h"
#include "Tools.h"
#include "Types.h"

#define DEFAULT_BUFLEN 8192
#define DEFAULT_PORT "27015"
//#define BOT_DIR "/home/crysison/Bots/"
#define BOT_DIR "/home/crypt/Bots/"
#define PORT_START 2903
#define PORT_END 2906


#define ZeroMemory(Destination,Length) memset((Destination),0,(Length))
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#define INVALID_SOCKET  (SOCKET)(~0)
#define SOCKET_ERROR            (-1)

static int conn_count = 0;
using namespace rapidjson;

static void ProcessClient(void* lpParameter)
{
	LMSocketConnection* SocketConnection = (LMSocketConnection*)lpParameter;
	SocketConnection->HandleConnection();
}

static void ShipData(const char* source, SOCKET ClientSocket, SOCKET ServerSocket, std::mutex* CritLock)
{
	char* recvbuf = (char*)malloc(DEFAULT_BUFLEN);
	if (!recvbuf)
	{
		return;
	}
	ssize_t iResult;
	int recvbuflen = DEFAULT_BUFLEN;
	bool bShouldExit = false;
	while (!bShouldExit)
	{
		ZeroMemory(recvbuf, recvbuflen);
		//		CritLock->lock();
		iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
		//		CritLock->unlock();
		if (iResult < 0)
		{
			int result = errno;
			if (result != EWOULDBLOCK)
			{
				printf("%s recv failed with error: %d\r\n", source, result);
				return;
			}
		}

		else if (iResult == 0)
		{
			printf("%s Connection close: %d\r\n", source, errno);
			bShouldExit = true;
		}

		else
		{
			if (conn_count > 0)
			{
				printf("%s Recieved %d bytes\r\n", source, iResult);
				printf("\r\n\r\n%s\r\n\r\n", recvbuf);
				conn_count --;
			}
			char* tmp_data = recvbuf;
			ssize_t data_remaining = iResult;

			while (data_remaining > 0 && !bShouldExit)
			{
				int pkt_data_size = min(data_remaining, DEFAULT_BUFLEN); // replace 1024 with whatever maximum chunk size you want...

				char* pkt = (char*)malloc(pkt_data_size);
				if (!pkt)
				{
					return;
				}
				ZeroMemory(pkt, pkt_data_size);
				// fill header as needed...
				memcpy(pkt, tmp_data, pkt_data_size);

				char* tmp_pkt = pkt;
				//				CritLock->lock();
				ssize_t Sresult = send(ServerSocket, tmp_pkt, pkt_data_size, 0);
				//				CritLock->unlock();
				if (Sresult == INVALID_SOCKET)
				{
					printf("%s Server Connection close\r\n", source);
					bShouldExit = true;
				}
				if (Sresult == INVALID_SOCKET)
				{
					bShouldExit = true;
				}
				else if (Sresult == SOCKET_ERROR)
				{
					int resultCode = errno;
					if (resultCode != EWOULDBLOCK)
					{
						printf("%s Unable to send data\r\n", source);
						bShouldExit = true;
					}
				}
				tmp_data += Sresult;
				data_remaining -= Sresult;
				free(pkt);
			}
			if (conn_count > 1)
			{
				printf("%s Sent %d bytes\r\n", source, tmp_data - recvbuf);
			}

		}
	}
	free(recvbuf);
}

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
		OutCmdLine = FileName;
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


int main(int argc, char** argv)
{
	LMSocketServer* Server = new LMSocketServer();
	Server->RunServer();
}


LMSocketServer::LMSocketServer()
{
	for (int i = PORT_START; i < PORT_END; i++)
	{
		PortMap[i] = nullptr;
	}
}

int LMSocketServer::RunServer()
{
	int iResult;

	SOCKET ListenSocket = INVALID_SOCKET;
	SOCKET ClientSocket = INVALID_SOCKET;

	struct addrinfo* result = NULL;
	struct addrinfo hints;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Resolve the server address and port
	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		return 1;
	}

	// Create a SOCKET for connecting to server
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", errno);
		freeaddrinfo(result);
		return 1;
	}

	// Setup the TCP listening socket
	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", errno);
		freeaddrinfo(result);
		close(ListenSocket);
		return 1;
	}

	freeaddrinfo(result);

	iResult = listen(ListenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", errno);
		close(ListenSocket);
		return 1;
	}
	bool bShouldExit = false;
	while (!bShouldExit)
	{
		ClientSocket = SOCKET_ERROR;

		while (ClientSocket == SOCKET_ERROR)
		{
			ClientSocket = accept(ListenSocket, NULL, NULL);
		}

		printf("Remote Client Connected.\n");

		int NewServerPort = 0;

		for (auto it = PortMap.begin(); it != PortMap.end(); it++)
		{
			if (it->second == nullptr)
			{
				NewServerPort = it->first;
				break;
			}
			/*
			if (it->second->joinable())
			{
				it->second = nullptr;
				NewServerPort = it->first;
				break;
			}
			*/
		}
		if (NewServerPort > 0)
		{
			LMSocketConnection* NewConnection = new LMSocketConnection(ClientSocket, NewServerPort);
			PortMap[NewServerPort] = new std::thread(&ProcessClient, (void*)NewConnection);
		}
		else
		{
			SendClientResponse(ClientSocket, false, "No available ports\r\n");
		}
	}


	// shutdown the connection since we're done
	iResult = shutdown(ClientSocket, SHUT_WR);
	if (iResult == SOCKET_ERROR) {
		printf("shutdown failed with error: %d\n", errno);
		close(ClientSocket);
		return 1;
	}

	// cleanup
	close(ClientSocket);

	return 0;
}

LMSocketConnection::LMSocketConnection(SOCKET InClientSocket, int InServerPort)
	: ClientSocket(InClientSocket)
	, ServerPort(InServerPort)
{

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

	bool bShouldExit = false;
	while (!bShouldExit)
	{
		ZeroMemory(recvbuf, DEFAULT_BUFLEN);

		recv(ClientSocket, recvbuf, DEFAULT_BUFLEN, 0);
		std::cout << "Client said: " << recvbuf << std::endl;
		Document RequestDoc;
		if (RequestDoc.Parse(recvbuf).HasParseError())
		{
			SendClientResponse(ClientSocket, false, "Invalid Json Document\r\n");
			bShouldExit = true;
			break;
		}

		if (!RequestDoc["BotName"].IsString())
		{
			SendClientResponse(ClientSocket, false, "Bot Name not found\r\n");
			bShouldExit = true;
			break;
		}

		if (!RequestDoc["StartPort"].IsString())
		{
			SendClientResponse(ClientSocket, false, "Start port not found\r\n");
			bShouldExit = true;
			break;
		}

		std::string  BotConfigLocation;
		std::string BotRootLocation = BOT_DIR;
		BotRootLocation += RequestDoc["BotName"].GetString();
		BotConfigLocation = BotRootLocation + "/ladderbots.json";
		if (!CheckExists(BotConfigLocation))
		{
			SendClientResponse(ClientSocket, false, "Bot not on server\r\n");
			bShouldExit = true;
			break;
		}

		Agents.LoadAgents(BotRootLocation, BotConfigLocation);
		BotConfig CurrentBotConfig;
		if (!Agents.FindBot(RequestDoc["BotName"].GetString(), CurrentBotConfig))
		{
			SendClientResponse(ClientSocket, false, "Invalid bot config\r\n");
			bShouldExit = true;
			break;
		}
		int StartPort = atoi(RequestDoc["StartPort"].GetString());
		SendClientResponse(ClientSocket, true, "Success");
		CurrentBotConfig.executeCommand = GetBotStartCommand(CurrentBotConfig.Type, CurrentBotConfig.FileName, "");
		std::string BotCommandLine = getBotCommandLine(CurrentBotConfig.executeCommand, ServerPort, StartPort, "HUMAN");
		RunBot(BotCommandLine, CurrentBotConfig);

	}
	free(recvbuf);
	return true;
}

void ListenForBot(LMSocketConnection* ServerSocket)
{
	int iResult;

	SOCKET ListenSocket = INVALID_SOCKET;
	SOCKET ClientSocket = INVALID_SOCKET;

	struct addrinfo* result = NULL;
	struct addrinfo hints;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Resolve the server address and port
	iResult = getaddrinfo(NULL, std::to_string(ServerSocket->ServerPort).c_str(), &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		return;
	}

	// Create a SOCKET for connecting to server
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", errno);
		freeaddrinfo(result);
		return;
	}

	// Setup the TCP listening socket
	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", errno);
		freeaddrinfo(result);
		close(ListenSocket);
		return;
	}

	freeaddrinfo(result);
	
	int option = 1;
	if (setsockopt(ListenSocket, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char*)&option, sizeof(option)) < 0)
	{
		printf("socket options failed with: %d\n", errno);
		close(ListenSocket);
		return;
	}

	iResult = listen(ListenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", errno);
		close(ListenSocket);
		return;
	}
	printf("listeing on port: %d\n", ServerSocket->ServerPort);

	ClientSocket = SOCKET_ERROR;

	while (ClientSocket == SOCKET_ERROR)
	{
		ClientSocket = accept(ListenSocket, NULL, NULL);
	}

	printf("Bot Client Connected.\n");

	std::mutex CritLock;
	conn_count = 6;
	std::thread ClientThread(&ShipData, "CLIENT", ClientSocket, ServerSocket->ClientSocket, &CritLock);
	std::thread Sc2Thread(&ShipData, "BOT", ServerSocket->ClientSocket, ClientSocket, &CritLock);

	ClientThread.join();
	Sc2Thread.join();


	// shutdown the connection since we're done
//	iResult = shutdown(ClientSocket, SHUT_WR);
//	if (iResult == SOCKET_ERROR) {
//		printf("shutdown failed with error: %d\n", errno);
//		close(ClientSocket);
//		return;
//	}
	// cleanup
	close(ClientSocket);

//	iResult = shutdown(ListenSocket, SHUT_WR);
//	if (iResult == SOCKET_ERROR) {
//		printf("shutdown failed with error: %d\n", errno);
//		close(ListenSocket);
//		return;
// 	}
	close(ListenSocket);
	return;
}


void LMSocketConnection::RunBot(std::string BotCommandLine, BotConfig CurrentBotConfig)
{
	unsigned long ProcessId;
	ListenServerTread = std::async(std::launch::async, &ListenForBot, this);
	sleep(1);
	printf("Starting bot with command line: %s", BotCommandLine.c_str());
	BotRunThread = std::async(std::launch::async, &StartBotProcess, CurrentBotConfig, BotCommandLine, &ProcessId);
	bool bShouldExit = false;
	while (!bShouldExit)
	{
		if (BotRunThread.wait_for(std::chrono::seconds(5)) == std::future_status::ready)
		{
			bShouldExit = true;
		}
	}
}


bool LMSocketConnection::ParseConfig(std::string ConfigFileLocation, Document& doc)
{
	std::ifstream t(ConfigFileLocation);
	if (t.good())
	{
		std::stringstream buffer;
		buffer << t.rdbuf();
		std::string ConfigString = buffer.str();
		return !doc.Parse(ConfigString.c_str()).HasParseError();
	}
	return false;
}
std::string LMSocketConnection::getBotCommandLine(const std::string CommandLine, int gamePort, const int startPort, const std::string& opponentID) const
{
	// Add universal arguments
	return CommandLine + " --GamePort " + std::to_string(gamePort) + " --StartPort " + std::to_string(startPort) + " --LadderServer 127.0.0.1 --OpponentId " + opponentID;
}
/*
int main(int argc, char** argv)
{
	BotConfig NewBotConfig;
	NewBotConfig.BotName = "MicroMachine";
	NewBotConfig.Debug = true;
	NewBotConfig.RootPath = "/home/crypt/DebugTest";
	unsigned long ProcessId = 0;
	StartBotProcess(NewBotConfig, "python /home/crypt/DebugTest/test.py klajd jklsajd askldjas sadkj", &ProcessId);
	printf("Waiting for exit");
	sleep(5);
}*/