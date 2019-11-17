#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <experimental/filesystem>
#include <string>
#include <fstream>
#include <sstream>
#include <future>
#include <mutex>
#include <thread>
#define RAPIDJSON_HAS_STDSTRING 1

#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include "LMHumanSockets.h"
#include "Tools.h"
#include "Types.h"
// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN 8192
#define DEFAULT_PORT "27015"
#define BOT_DIR "Bots/"
#define PORT_START 2903
#define PORT_END 2906
using namespace rapidjson;

namespace fs = std::experimental::filesystem;



DWORD WINAPI ProcessClient(LPVOID lpParameter)
{
	LMSocketConnection* SocketConnection = (LMSocketConnection*)lpParameter;
	SocketConnection->HandleConnection();
	return 0;
}

static void ShipData(const char* source, SOCKET ClientSocket, SOCKET ServerSocket, std::mutex* CritLock)
{
	char* recvbuf = (char*)malloc(DEFAULT_BUFLEN);
	if (!recvbuf)
	{
		return;
	}
	int iResult;
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
			int result = WSAGetLastError();
			if (result != WSAEWOULDBLOCK)
			{
				printf("%s recv failed with error: %d\r\n", source, WSAGetLastError());
				return;
			}
		}

		else if (iResult == 0)
		{
			printf("%s Connection close\r\n", source);
			bShouldExit = true;
		}

		else
		{
//			printf("%s Recieved %d bytes\r\n", source, iResult);
//			printf("\r\n\r\n%s\r\n\r\n", recvbuf);
			char* tmp_data = recvbuf;
			int data_remaining = iResult;

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
				int Sresult = send(ServerSocket, tmp_pkt, pkt_data_size, 0);
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
					int resultCode = WSAGetLastError();
					if (resultCode != WSAEWOULDBLOCK)
					{
						bShouldExit = true;
					}
				}
				tmp_data += Sresult;
				data_remaining -= Sresult;
				free(pkt);
			}
//			printf("%s Sent %d bytes\r\n", source, tmp_data - recvbuf);

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
		OutCmdLine = "Python " + FileName;
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
	for (int i = PORT_START; i < PORT_END; i++ )
	{
		PortMap[i] = nullptr;
	}
}

int LMSocketServer::RunServer()
{
	WSADATA wsaData;
	int iResult;

	SOCKET ListenSocket = INVALID_SOCKET;
	SOCKET ClientSocket = INVALID_SOCKET;

	struct addrinfo* result = NULL;
	struct addrinfo hints;

	int recvbuflen = DEFAULT_BUFLEN;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\r\n", iResult);
		return 1;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Resolve the server address and port
	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	// Create a SOCKET for connecting to server
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	// Setup the TCP listening socket
	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	freeaddrinfo(result);

	iResult = listen(ListenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
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

		DWORD dwThreadId;
		int NewServerPort = 0;

		for (auto it = PortMap.begin(); it != PortMap.end(); it++)
		{
			if (it->second == nullptr)
			{
				NewServerPort = it->first;
				break;
			}
		}
		if (NewServerPort > 0)
		{
			LMSocketConnection* NewConnection = new LMSocketConnection(ClientSocket, NewServerPort);
			PortMap[NewServerPort] = NewConnection;
			CreateThread(NULL, 0, ProcessClient, (LPVOID)NewConnection, 0, &dwThreadId);
		}
		else
		{
			SendClientResponse(ClientSocket, false, "No available ports\r\n");
		}
	}


	// shutdown the connection since we're done
	iResult = shutdown(ClientSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		WSACleanup();
		return 1;
	}

	// cleanup
	closesocket(ClientSocket);
	WSACleanup();

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
	int bytesRecv = SOCKET_ERROR;
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

		bytesRecv = recv(ClientSocket, recvbuf, DEFAULT_BUFLEN, 0);
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
		if (!fs::exists(BotConfigLocation))
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

	int recvbuflen = DEFAULT_BUFLEN;

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
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		return;
	}

	// Setup the TCP listening socket
	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		return;
	}

	freeaddrinfo(result);

	iResult = listen(ListenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		return;
	}

	ClientSocket = SOCKET_ERROR;

	while (ClientSocket == SOCKET_ERROR)
	{
		ClientSocket = accept(ListenSocket, NULL, NULL);
	}

	printf("Bot Client Connected.\n");

	std::mutex CritLock;
	std::thread ClientThread(&ShipData, "CLIENT", ClientSocket, ServerSocket->ClientSocket, &CritLock);
	std::thread Sc2Thread(&ShipData, "BOT", ServerSocket->ClientSocket, ClientSocket, &CritLock);

	ClientThread.join();
	Sc2Thread.join();


	// shutdown the connection since we're done
	iResult = shutdown(ClientSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		return;
	}

	// cleanup
	closesocket(ClientSocket);
	return;
}


void LMSocketConnection::RunBot(std::string BotCommandLine, BotConfig CurrentBotConfig)
{
	unsigned long ProcessId;
	ListenServerTread = std::async(std::launch::async, &ListenForBot, this);
	Sleep(1000);
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
std::string LMSocketConnection::getBotCommandLine(const std::string CommandLine,  int gamePort, const int startPort, const std::string& opponentID) const
{
	// Add universal arguments
	return CommandLine + " --GamePort " + std::to_string(gamePort) + " --StartPort " + std::to_string(startPort) + " --LadderServer 127.0.0.1 --OpponentId " + opponentID;
}
