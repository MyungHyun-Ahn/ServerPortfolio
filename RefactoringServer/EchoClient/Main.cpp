#include "Pch.h"

#include "Crypto/FDefaultPacketCipher.h"
#include "Packet/FDefaultPacketFramer.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <thread>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")

namespace
{
	constexpr std::uint8_t kPacketKey = 0x37;
	constexpr std::uint16_t kEchoRequestOpcode = 1000;
	constexpr std::uint16_t kEchoResponseOpcode = 1001;

	struct SClientOptions
	{
		std::string serverIp = "127.0.0.1";
		std::uint16_t port = 19000;
		int requestCount = 1;
		int payloadSize = 9;
		int sendChunkSize = 0;
		int sendChunkDelayMs = 0;
		int recvBufferSize = 32;
		bool verbose = true;
	};

	bool TryParseInt(const char* valueText, int& outValue)
	{
		if (valueText == nullptr)
		{
			return false;
		}

		char* parseEnd = nullptr;
		const long parsedValue = std::strtol(valueText, &parseEnd, 10);
		if (parseEnd == valueText || *parseEnd != '\0')
		{
			return false;
		}

		outValue = static_cast<int>(parsedValue);
		return true;
	}

	bool ParseArguments(int argc, char* argv[], SClientOptions& outOptions)
	{
		for (int argumentIndex = 1; argumentIndex < argc; ++argumentIndex)
		{
			const std::string argument = argv[argumentIndex];
			if (argument == "--server-ip" && argumentIndex + 1 < argc)
			{
				outOptions.serverIp = argv[++argumentIndex];
			}
			else if (argument == "--port" && argumentIndex + 1 < argc)
			{
				int parsedValue = 0;
				if (!TryParseInt(argv[++argumentIndex], parsedValue) || parsedValue <= 0 || parsedValue > 65535)
				{
					return false;
				}

				outOptions.port = static_cast<std::uint16_t>(parsedValue);
			}
			else if (argument == "--count" && argumentIndex + 1 < argc)
			{
				if (!TryParseInt(argv[++argumentIndex], outOptions.requestCount) || outOptions.requestCount <= 0)
				{
					return false;
				}
			}
			else if (argument == "--payload-size" && argumentIndex + 1 < argc)
			{
				if (!TryParseInt(argv[++argumentIndex], outOptions.payloadSize) || outOptions.payloadSize <= 0)
				{
					return false;
				}
			}
			else if (argument == "--send-chunk-size" && argumentIndex + 1 < argc)
			{
				if (!TryParseInt(argv[++argumentIndex], outOptions.sendChunkSize) || outOptions.sendChunkSize < 0)
				{
					return false;
				}
			}
			else if (argument == "--send-chunk-delay-ms" && argumentIndex + 1 < argc)
			{
				if (!TryParseInt(argv[++argumentIndex], outOptions.sendChunkDelayMs) || outOptions.sendChunkDelayMs < 0)
				{
					return false;
				}
			}
			else if (argument == "--recv-buffer-size" && argumentIndex + 1 < argc)
			{
				if (!TryParseInt(argv[++argumentIndex], outOptions.recvBufferSize) || outOptions.recvBufferSize <= 0)
				{
					return false;
				}
			}
			else if (argument == "--quiet")
			{
				outOptions.verbose = false;
			}
			else
			{
				return false;
			}
		}

		return true;
	}

	std::string BuildRequestMessage(int requestIndex, int payloadSize)
	{
		std::string message = "echo-test-" + std::to_string(requestIndex);
		if (payloadSize <= static_cast<int>(message.size()))
		{
			message.resize(static_cast<std::size_t>(payloadSize));
			return message;
		}

		while (static_cast<int>(message.size()) < payloadSize)
		{
			const char padCharacter = static_cast<char>('a' + (requestIndex % 26));
			message.push_back(padCharacter);
		}

		return message;
	}

	bool SendFully(SOCKET socketHandle, const char* buffer, int length)
	{
		int totalSent = 0;
		while (totalSent < length)
		{
			const int sentBytes = send(socketHandle, buffer + totalSent, length - totalSent, 0);
			if (sentBytes == SOCKET_ERROR || sentBytes == 0)
			{
				return false;
			}

			totalSent += sentBytes;
		}

		return true;
	}

	bool SendPacketWithOptionalChunking(SOCKET socketHandle, const std::vector<char>& packetBuffer, int chunkSize, int chunkDelayMs)
	{
		if (chunkSize <= 0 || chunkSize >= static_cast<int>(packetBuffer.size()))
		{
			return SendFully(socketHandle, packetBuffer.data(), static_cast<int>(packetBuffer.size()));
		}

		int offset = 0;
		while (offset < static_cast<int>(packetBuffer.size()))
		{
			const int bytesToSend = std::min(chunkSize, static_cast<int>(packetBuffer.size()) - offset);
			if (!SendFully(socketHandle, packetBuffer.data() + offset, bytesToSend))
			{
				return false;
			}

			offset += bytesToSend;
			if (offset < static_cast<int>(packetBuffer.size()) && chunkDelayMs > 0)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(chunkDelayMs));
			}
		}

		return true;
	}
}

int main(int argc, char* argv[])
{
	SClientOptions options{};
	if (!ParseArguments(argc, argv, options))
	{
		std::cerr << "usage: EchoClient.exe [--server-ip 127.0.0.1] [--port 19000] [--count 10] [--payload-size 64] [--send-chunk-size 8] [--send-chunk-delay-ms 1] [--recv-buffer-size 16] [--quiet]\n";
		return 1;
	}

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
	serverAddress.sin_port = htons(options.port);
	InetPtonA(AF_INET, options.serverIp.c_str(), &serverAddress.sin_addr);

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

	std::vector<std::string> expectedMessages;
	expectedMessages.reserve(static_cast<std::size_t>(options.requestCount));

	for (int requestIndex = 0; requestIndex < options.requestCount; ++requestIndex)
	{
		const std::string requestMessage = BuildRequestMessage(requestIndex, options.payloadSize);
		expectedMessages.push_back(requestMessage);

		const std::uint8_t requestRandomKey = static_cast<std::uint8_t>((0x31 + requestIndex) & 0xFF);
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
			std::cerr << "BuildPacket failed for request " << requestIndex << ".\n";
			closesocket(clientSocket);
			WSACleanup();
			return 1;
		}

		if (!SendPacketWithOptionalChunking(clientSocket, outboundPacket, options.sendChunkSize, options.sendChunkDelayMs))
		{
			std::cerr << "send failed for request " << requestIndex << ". error=" << WSAGetLastError() << "\n";
			closesocket(clientSocket);
			WSACleanup();
			return 1;
		}
	}

	std::vector<char> inboundBuffer;
	inboundBuffer.reserve(static_cast<std::size_t>(options.recvBufferSize) * 2);
	std::vector<char> recvChunk(static_cast<std::size_t>(options.recvBufferSize));
	int receivedResponseCount = 0;

	while (receivedResponseCount < options.requestCount)
	{
		const int recvBytes = recv(clientSocket, recvChunk.data(), static_cast<int>(recvChunk.size()), 0);
		if (recvBytes <= 0)
		{
			std::cerr << "recv failed before all responses arrived.\n";
			closesocket(clientSocket);
			WSACleanup();
			return 1;
		}

		inboundBuffer.insert(inboundBuffer.end(), recvChunk.begin(), recvChunk.begin() + recvBytes);

		while (true)
		{
			GameServer::NetworkLib::Packet::SFramedPacket framedPacket;
			if (!packetFramer.TryExtractPacket(inboundBuffer, framedPacket))
			{
				break;
			}

			const std::uint8_t responseChecksum =
				GameServer::NetworkLib::Packet::CalculatePacketChecksum(
					framedPacket.payload.data(),
					static_cast<std::int32_t>(framedPacket.payload.size()));
			if (responseChecksum != framedPacket.checkSum)
			{
				std::cerr << "packet checksum failed for response " << receivedResponseCount << ".\n";
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

			if (responseMessage != expectedMessages[static_cast<std::size_t>(receivedResponseCount)])
			{
				std::cerr << "echo validation failed at response " << receivedResponseCount << ". expected="
					<< expectedMessages[static_cast<std::size_t>(receivedResponseCount)] << " actual=" << responseMessage << "\n";
				closesocket(clientSocket);
				WSACleanup();
				return 1;
			}

			if (options.verbose)
			{
				std::cout << "response[" << receivedResponseCount << "]: " << responseMessage << "\n";
			}

			++receivedResponseCount;
		}
	}

	shutdown(clientSocket, SD_BOTH);
	closesocket(clientSocket);
	WSACleanup();

	std::cout << "echo validation succeeded. responses=" << receivedResponseCount
		<< " payloadSize=" << options.payloadSize
		<< " sendChunkSize=" << options.sendChunkSize
		<< " recvBufferSize=" << options.recvBufferSize << "\n";
	return 0;
}
