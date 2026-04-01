#include "Pch.h"

#include "Crypto/FDefaultPacketCipher.h"

#pragma comment(lib, "Ws2_32.lib")

int main()
{
	constexpr std::uint8_t kPacketKey = 0x37;
	constexpr std::uint8_t kRequestRandomKey = 0x5A;

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

	GameServer::NetworkLib::Crypto::SDefaultPacketCipherConfig cipherConfig{};
	cipherConfig.packetKey = kPacketKey;
	GameServer::NetworkLib::Crypto::FDefaultPacketCipher packetCipher(cipherConfig);

	const std::string requestMessage = "echo-test";
	std::string encryptedRequest;
	encryptedRequest.resize(requestMessage.size() + 1);
	encryptedRequest[0] = static_cast<char>(kRequestRandomKey);
	std::copy(requestMessage.begin(), requestMessage.end(), encryptedRequest.begin() + 1);
	packetCipher.Encode(encryptedRequest.data() + 1, static_cast<int>(requestMessage.size()), kRequestRandomKey);

	if (send(clientSocket, encryptedRequest.data(), static_cast<int>(encryptedRequest.size()), 0) == SOCKET_ERROR)
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

	if (recvBytes <= 1)
	{
		std::cerr << "recv payload too short.\n";
		closesocket(clientSocket);
		WSACleanup();
		return 1;
	}

	const std::uint8_t responseRandomKey = static_cast<std::uint8_t>(recvBuffer[0]);
	std::string responseMessage(recvBuffer + 1, recvBuffer + recvBytes);
	packetCipher.Decode(responseMessage.data(), static_cast<int>(responseMessage.size()), responseRandomKey);
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
