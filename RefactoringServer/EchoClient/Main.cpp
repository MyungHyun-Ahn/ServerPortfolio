#include <WinSock2.h>
#include <WS2tcpip.h>

#include <iostream>
#include <string>

#pragma comment(lib, "Ws2_32.lib")

int main()
{
	WSADATA wsaData{};
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		std::cerr << "WSAStartup failed.\n";
		return 1;
	}

	SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (clientSocket == INVALID_SOCKET)
	{
		std::cerr << "socket creation failed.\n";
		WSACleanup();
		return 1;
	}

	sockaddr_in serverAddress{};
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(19000);
	InetPtonA(AF_INET, "127.0.0.1", &serverAddress.sin_addr);

	if (connect(clientSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) == SOCKET_ERROR)
	{
		std::cerr << "connect failed: " << WSAGetLastError() << "\n";
		closesocket(clientSocket);
		WSACleanup();
		return 1;
	}

	const std::string requestMessage = "echo-test";
	if (send(clientSocket, requestMessage.data(), static_cast<int>(requestMessage.size()), 0) == SOCKET_ERROR)
	{
		std::cerr << "send failed.\n";
		closesocket(clientSocket);
		WSACleanup();
		return 1;
	}

	char recvBuffer[128]{};
	const int recvBytes = recv(clientSocket, recvBuffer, static_cast<int>(sizeof(recvBuffer)), 0);
	if (recvBytes <= 0)
	{
		std::cerr << "recv failed.\n";
		closesocket(clientSocket);
		WSACleanup();
		return 1;
	}

	const std::string responseMessage(recvBuffer, recvBuffer + recvBytes);
	std::cout << "response: " << responseMessage << "\n";

	shutdown(clientSocket, SD_BOTH);
	closesocket(clientSocket);
	WSACleanup();

	if (responseMessage != requestMessage)
	{
		std::cerr << "echo validation failed.\n";
		return 1;
	}

	std::cout << "echo validation succeeded.\n";
	return 0;
}
