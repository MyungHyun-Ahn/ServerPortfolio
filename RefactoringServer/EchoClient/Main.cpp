#include "Pch.h"

#include "Crypto/FDefaultPacketCipher.h"
#include "Packet/FDefaultPacketFramer.h"

#include <vector>

#pragma comment(lib, "Ws2_32.lib")

int main()
{
	constexpr std::uint8_t kPacketKey = 0x37;
	constexpr std::uint16_t kEchoRequestOpcode = 1000;
	constexpr std::uint16_t kEchoResponseOpcode = 1001;

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
	GameServer::NetworkLib::Packet::FDefaultPacketFramer packetFramer;

	const std::string requestMessage = "echo-test";
	const std::uint8_t requestRandomKey = 0x5A;
	std::vector<char> encryptedPayload(requestMessage.begin(), requestMessage.end());
	packetCipher.Encode(encryptedPayload.data(), static_cast<int>(encryptedPayload.size()), requestRandomKey);

	GameServer::NetworkLib::Packet::SOutgoingPacket outgoingPacket{};
	outgoingPacket.opcode = kEchoRequestOpcode;
	outgoingPacket.randomKey = requestRandomKey;
	outgoingPacket.checkSum =
		GameServer::NetworkLib::Packet::CalculatePacketChecksum(
			encryptedPayload.data(),
			static_cast<std::int32_t>(encryptedPayload.size()));
	outgoingPacket.payload = encryptedPayload.data();
	outgoingPacket.payloadLength = static_cast<std::int32_t>(encryptedPayload.size());

	std::vector<char> outboundPacket;
	if (!packetFramer.BuildPacket(outgoingPacket, outboundPacket))
	{
		std::cerr << "BuildPacket failed.\n";
		closesocket(clientSocket);
		WSACleanup();
		return 1;
	}

	if (send(clientSocket, outboundPacket.data(), static_cast<int>(outboundPacket.size()), 0) == SOCKET_ERROR)
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

	std::vector<char> inboundBuffer(recvBuffer, recvBuffer + recvBytes);
	GameServer::NetworkLib::Packet::SFramedPacket framedPacket;
	if (!packetFramer.TryExtractPacket(inboundBuffer, framedPacket))
	{
		std::cerr << "packet framing failed.\n";
		closesocket(clientSocket);
		WSACleanup();
		return 1;
	}

	const std::uint8_t responseChecksum =
		GameServer::NetworkLib::Packet::CalculatePacketChecksum(
			framedPacket.payload.data(),
			static_cast<std::int32_t>(framedPacket.payload.size()));
	if (responseChecksum != framedPacket.checkSum)
	{
		std::cerr << "packet checksum failed.\n";
		closesocket(clientSocket);
		WSACleanup();
		return 1;
	}

	if (framedPacket.opcode != kEchoResponseOpcode)
	{
		std::cerr << "unexpected opcode: " << framedPacket.opcode << "\n";
		closesocket(clientSocket);
		WSACleanup();
		return 1;
	}

	std::string responseMessage(framedPacket.payload.begin(), framedPacket.payload.end());
	packetCipher.Decode(responseMessage.data(), static_cast<int>(responseMessage.size()), framedPacket.randomKey);
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
