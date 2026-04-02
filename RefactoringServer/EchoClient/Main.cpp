#include "Pch.h"

#include "Crypto/FDefaultPacketCipher.h"
#include "Generated/Packets/Chat/ChatPackets.h"
#include "Generated/Packets/Echo/EchoPackets.h"
#include "Generated/Packets/Login/LoginPackets.h"
#include "Packet/Buffer/FPacketBuffer.h"
#include "Packet/Framing/FDefaultPacketFramer.h"
#include "Packet/Framing/PacketTypes.h"
#include "Packet/Serialization/FPacketSerialization.h"
#include "Packet/View/FPacketView.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <random>
#include <unordered_map>
#include <thread>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")

namespace
{
	constexpr std::uint8_t kPacketKey = 0x37;

	struct SClientOptions
	{
		std::string serverIp = "127.0.0.1";
		std::uint16_t port = 19000;
		std::uint32_t loginUserIdBase = 1000;
		int sessionCount = 1;
		int requestCount = 1;
		int payloadSize = 9;
		int sendChunkSize = 0;
		int sendChunkDelayMs = 0;
		int recvBufferSize = 32;
		int responseThreadCount = 1;
		int responsesPerThread = 1;
		int holdSeconds = 0;
		int intervalMs = 1000;
		int packetsPerSend = 1;
		int reconnectProbabilityPercent = 0;
		int reconnectDelayMs = 100;
		int recvTimeoutMs = 0;
		bool enablePagePool = true;
		int pageSize = 4096;
		bool bootstrapTrace = false;
		bool verbose = true;
	};

	struct SSessionResult
	{
		bool succeeded = false;
		int receivedResponseCount = 0;
		std::string errorMessage;
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
			else if (argument == "--login-userid-base" && argumentIndex + 1 < argc)
			{
				int parsedValue = 0;
				if (!TryParseInt(argv[++argumentIndex], parsedValue) || parsedValue <= 0)
				{
					return false;
				}

				outOptions.loginUserIdBase = static_cast<std::uint32_t>(parsedValue);
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
			else if (argument == "--sessions" && argumentIndex + 1 < argc)
			{
				if (!TryParseInt(argv[++argumentIndex], outOptions.sessionCount) || outOptions.sessionCount <= 0)
				{
					return false;
				}
			}
			else if (argument == "--response-thread-count" && argumentIndex + 1 < argc)
			{
				if (!TryParseInt(argv[++argumentIndex], outOptions.responseThreadCount) || outOptions.responseThreadCount <= 0)
				{
					return false;
				}
			}
			else if (argument == "--responses-per-thread" && argumentIndex + 1 < argc)
			{
				if (!TryParseInt(argv[++argumentIndex], outOptions.responsesPerThread) || outOptions.responsesPerThread <= 0)
				{
					return false;
				}
			}
			else if (argument == "--hold-seconds" && argumentIndex + 1 < argc)
			{
				if (!TryParseInt(argv[++argumentIndex], outOptions.holdSeconds) || outOptions.holdSeconds < 0)
				{
					return false;
				}
			}
			else if (argument == "--interval-ms" && argumentIndex + 1 < argc)
			{
				if (!TryParseInt(argv[++argumentIndex], outOptions.intervalMs) || outOptions.intervalMs < 0)
				{
					return false;
				}
			}
			else if (argument == "--packets-per-send" && argumentIndex + 1 < argc)
			{
				if (!TryParseInt(argv[++argumentIndex], outOptions.packetsPerSend) || outOptions.packetsPerSend <= 0)
				{
					return false;
				}
			}
			else if (argument == "--reconnect-probability-percent" && argumentIndex + 1 < argc)
			{
				if (!TryParseInt(argv[++argumentIndex], outOptions.reconnectProbabilityPercent) || outOptions.reconnectProbabilityPercent < 0 || outOptions.reconnectProbabilityPercent > 100)
				{
					return false;
				}
			}
			else if (argument == "--reconnect-delay-ms" && argumentIndex + 1 < argc)
			{
				if (!TryParseInt(argv[++argumentIndex], outOptions.reconnectDelayMs) || outOptions.reconnectDelayMs < 0)
				{
					return false;
				}
			}
			else if (argument == "--recv-timeout-ms" && argumentIndex + 1 < argc)
			{
				if (!TryParseInt(argv[++argumentIndex], outOptions.recvTimeoutMs) || outOptions.recvTimeoutMs < 0)
				{
					return false;
				}
			}
			else if (argument == "--disable-page-pool")
			{
				outOptions.enablePagePool = false;
			}
			else if (argument == "--page-size" && argumentIndex + 1 < argc)
			{
				if (!TryParseInt(argv[++argumentIndex], outOptions.pageSize) || outOptions.pageSize <= 0)
				{
					return false;
				}
			}
			else if (argument == "--quiet")
			{
				outOptions.verbose = false;
			}
			else if (argument == "--bootstrap-trace")
			{
				outOptions.bootstrapTrace = true;
			}
			else
			{
				return false;
			}
		}

		return true;
	}

	std::string BuildRequestMessage(int sessionIndex, int requestIndex, int payloadSize)
	{
		std::string message =
			"echo-s" + std::to_string(sessionIndex) + "-r" + std::to_string(requestIndex);
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

	std::vector<std::string> BuildExpectedResponseMessages(const std::string& requestMessage, const SClientOptions& options)
	{
		if (options.responseThreadCount == 1 && options.responsesPerThread == 1)
		{
			return { requestMessage };
		}

		std::vector<std::string> expectedResponses;
		expectedResponses.reserve(static_cast<std::size_t>(options.responseThreadCount * options.responsesPerThread));
		for (int threadIndex = 0; threadIndex < options.responseThreadCount; ++threadIndex)
		{
			for (int responseIndex = 0; responseIndex < options.responsesPerThread; ++responseIndex)
			{
				std::ostringstream responseBuilder;
				responseBuilder << requestMessage
					<< "|t=" << threadIndex
					<< "|r=" << responseIndex;
				expectedResponses.push_back(responseBuilder.str());
			}
		}

		return expectedResponses;
	}

	bool TryConnectSocket(const SClientOptions& options, SOCKET& outSocket, std::string& outErrorMessage)
	{
		outSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (outSocket == INVALID_SOCKET)
		{
			outErrorMessage = "socket creation failed.";
			return false;
		}

		sockaddr_in serverAddress{};
		serverAddress.sin_family = AF_INET;
		serverAddress.sin_port = htons(options.port);
		InetPtonA(AF_INET, options.serverIp.c_str(), &serverAddress.sin_addr);

		if (connect(outSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) == SOCKET_ERROR)
		{
			std::ostringstream oss;
			oss << "connect failed: " << WSAGetLastError();
			outErrorMessage = oss.str();
			closesocket(outSocket);
			outSocket = INVALID_SOCKET;
			return false;
		}

		if (options.recvTimeoutMs > 0)
		{
			const DWORD timeoutMs = static_cast<DWORD>(options.recvTimeoutMs);
			if (setsockopt(outSocket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs)) == SOCKET_ERROR)
			{
				std::ostringstream oss;
				oss << "setsockopt(SO_RCVTIMEO) failed: " << WSAGetLastError();
				outErrorMessage = oss.str();
				closesocket(outSocket);
				outSocket = INVALID_SOCKET;
				return false;
			}
		}

		return true;
	}

	bool ShouldReconnect(const SClientOptions& options, std::mt19937& randomEngine)
	{
		if (options.reconnectProbabilityPercent <= 0)
		{
			return false;
		}

		std::uniform_int_distribution<int> distribution(1, 100);
		return distribution(randomEngine) <= options.reconnectProbabilityPercent;
	}

	SSessionResult RunSingleSession(int sessionIndex, const SClientOptions& options)
	{
		SSessionResult sessionResult{};
		NetworkLib::Crypto::SDefaultPacketCipherConfig cipherConfig{};
		cipherConfig.packetKey = kPacketKey;
		NetworkLib::Crypto::FDefaultPacketCipher packetCipher(cipherConfig);
		NetworkLib::Packet::Framing::FDefaultPacketFramer packetFramer;
		const auto startTime = std::chrono::steady_clock::now();
		const auto deadline =
			startTime + std::chrono::seconds(options.holdSeconds > 0 ? options.holdSeconds : 0);
		int requestSequence = 0;
		std::mt19937 randomEngine(static_cast<std::uint32_t>(GetTickCount64()) ^ static_cast<std::uint32_t>(sessionIndex * 2654435761u));
		SOCKET clientSocket = INVALID_SOCKET;
		bool requiresBootstrapAfterConnect = true;

		while (true)
		{
			if (clientSocket == INVALID_SOCKET)
			{
				if (!TryConnectSocket(options, clientSocket, sessionResult.errorMessage))
				{
					return sessionResult;
				}

				requiresBootstrapAfterConnect = true;
			}

			std::vector<char> inboundBuffer;
			inboundBuffer.reserve(static_cast<std::size_t>(options.recvBufferSize) * 2);
			std::vector<char> recvChunk(static_cast<std::size_t>(options.recvBufferSize));
			std::unordered_map<std::string, int> expectedResponseCounts;
			std::vector<char> sendBatchBuffer;
			auto tryReceiveNextContentPacket =
				[&](
					const char* stageName,
					NetworkLib::Packet::Framing::SFramedPacket& outFramedPacket,
					NetworkLib::Packet::View::FPacketView& outContentPacketView) -> bool
				{
					while (true)
					{
						if (packetFramer.TryExtractPacket(inboundBuffer, outFramedPacket))
						{
							const std::uint8_t responseChecksum =
								NetworkLib::Packet::Framing::CalculatePacketChecksum(
									outFramedPacket.payload.data(),
									static_cast<std::int32_t>(outFramedPacket.payload.size()));
							if (responseChecksum != outFramedPacket.checkSum)
							{
								sessionResult.errorMessage = "packet checksum failed.";
								return false;
							}

							packetCipher.Decode(outFramedPacket.payload.data(), static_cast<int>(outFramedPacket.payload.size()), outFramedPacket.randomKey);

							NetworkLib::Packet::View::FPacketView transportPacketView{};
							transportPacketView.randomKey = outFramedPacket.randomKey;
							transportPacketView.checkSum = outFramedPacket.checkSum;
							transportPacketView.payload = outFramedPacket.payload.data();
							transportPacketView.payloadLength = static_cast<std::int32_t>(outFramedPacket.payload.size());

							if (!NetworkLib::Packet::Serialization::TryParseContentPacketView(transportPacketView, outContentPacketView))
							{
								sessionResult.errorMessage = "response content header parse failed.";
								return false;
							}

							return true;
						}

						const int recvBytes = recv(clientSocket, recvChunk.data(), static_cast<int>(recvChunk.size()), 0);
						if (recvBytes <= 0)
						{
							const int errorCode = WSAGetLastError();
							std::ostringstream oss;
							oss << "recv failed at stage="
								<< (stageName != nullptr ? stageName : "unknown")
								<< " sessionIndex=" << sessionIndex
								<< " error=" << errorCode;
							if (errorCode == WSAETIMEDOUT)
							{
								oss << " (timeout)";
							}
							sessionResult.errorMessage = oss.str();
							return false;
						}

						inboundBuffer.insert(inboundBuffer.end(), recvChunk.begin(), recvChunk.begin() + recvBytes);
					}
				};

			if (requiresBootstrapAfterConnect)
			{
				Generated::Login::FLoginRq loginRequest;
				loginRequest.userId = options.loginUserIdBase + static_cast<std::uint32_t>(sessionIndex);

				std::vector<char> loginPayload = NetworkLib::Packet::Serialization::SerializeContentPacket(loginRequest);
				const std::uint8_t loginRandomKey = static_cast<std::uint8_t>((0x21 + sessionIndex) & 0xFF);
				packetCipher.Encode(loginPayload.data(), static_cast<int>(loginPayload.size()), loginRandomKey);

				NetworkLib::Packet::Framing::SOutgoingPacket loginOutgoingPacket{};
				loginOutgoingPacket.randomKey = loginRandomKey;
				loginOutgoingPacket.checkSum =
					NetworkLib::Packet::Framing::CalculatePacketChecksum(
						loginPayload.data(),
						static_cast<std::int32_t>(loginPayload.size()));
				loginOutgoingPacket.payload = loginPayload.data();
				loginOutgoingPacket.payloadLength = static_cast<std::int32_t>(loginPayload.size());

				std::vector<char> loginWirePacket;
				if (!packetFramer.BuildPacket(loginOutgoingPacket, loginWirePacket))
				{
					sessionResult.errorMessage = "BuildPacket failed for login request.";
					closesocket(clientSocket);
					clientSocket = INVALID_SOCKET;
					return sessionResult;
				}

				if (!SendPacketWithOptionalChunking(clientSocket, loginWirePacket, options.sendChunkSize, options.sendChunkDelayMs))
				{
					sessionResult.errorMessage = "send failed for login request.";
					closesocket(clientSocket);
					clientSocket = INVALID_SOCKET;
					return sessionResult;
				}

				NetworkLib::Packet::Framing::SFramedPacket loginResponseFramedPacket{};
				NetworkLib::Packet::View::FPacketView loginResponsePacketView{};
				if (!tryReceiveNextContentPacket("login-response", loginResponseFramedPacket, loginResponsePacketView))
				{
					closesocket(clientSocket);
					clientSocket = INVALID_SOCKET;
					return sessionResult;
				}

				if (loginResponsePacketView.opcode != Generated::Login::FLoginRp::kOpcode)
				{
					std::ostringstream oss;
					oss << "unexpected login response opcode: " << loginResponsePacketView.opcode;
					sessionResult.errorMessage = oss.str();
					closesocket(clientSocket);
					clientSocket = INVALID_SOCKET;
					return sessionResult;
				}

				Generated::Login::FLoginRp loginResponse;
				if (!NetworkLib::Packet::Serialization::DeserializeContentPacket(loginResponsePacketView, loginResponse))
				{
					sessionResult.errorMessage = "login response deserialize failed.";
					closesocket(clientSocket);
					clientSocket = INVALID_SOCKET;
					return sessionResult;
				}

				if (!loginResponse.success || loginResponse.userId != loginRequest.userId)
				{
					sessionResult.errorMessage = "login validation failed.";
					closesocket(clientSocket);
					clientSocket = INVALID_SOCKET;
					return sessionResult;
				}

				if (options.bootstrapTrace)
				{
					std::cerr << "bootstrap trace: login response ok. sessionIndex=" << sessionIndex
						<< " userId=" << loginResponse.userId << "\n";
				}

				const std::uint32_t roomId = 77u + static_cast<std::uint32_t>(sessionIndex);
				Generated::Chat::FRoomSnapshotRq snapshotRequest;
				snapshotRequest.roomId = roomId;

				std::vector<char> serializedPayload = NetworkLib::Packet::Serialization::SerializeContentPacket(snapshotRequest);
				const std::uint8_t requestRandomKey = static_cast<std::uint8_t>((0x61 + sessionIndex) & 0xFF);
				packetCipher.Encode(serializedPayload.data(), static_cast<int>(serializedPayload.size()), requestRandomKey);

				NetworkLib::Packet::Framing::SOutgoingPacket outgoingPacket{};
				outgoingPacket.randomKey = requestRandomKey;
				outgoingPacket.checkSum =
					NetworkLib::Packet::Framing::CalculatePacketChecksum(
						serializedPayload.data(),
						static_cast<std::int32_t>(serializedPayload.size()));
				outgoingPacket.payload = serializedPayload.data();
				outgoingPacket.payloadLength = static_cast<std::int32_t>(serializedPayload.size());

				std::vector<char> outboundPacket;
				if (!packetFramer.BuildPacket(outgoingPacket, outboundPacket))
				{
					sessionResult.errorMessage = "BuildPacket failed for chat snapshot request.";
					closesocket(clientSocket);
					clientSocket = INVALID_SOCKET;
					return sessionResult;
				}

				if (!SendPacketWithOptionalChunking(clientSocket, outboundPacket, options.sendChunkSize, options.sendChunkDelayMs))
				{
					sessionResult.errorMessage = "send failed for chat snapshot request.";
					closesocket(clientSocket);
					clientSocket = INVALID_SOCKET;
					return sessionResult;
				}

				if (options.bootstrapTrace)
				{
					std::cerr << "bootstrap trace: snapshot request sent. sessionIndex=" << sessionIndex
						<< " roomId=" << roomId << "\n";
				}

				bool receivedSnapshotResponse = false;
				bool receivedBinarySnapshot = false;
				while (!receivedSnapshotResponse || !receivedBinarySnapshot)
				{
					NetworkLib::Packet::Framing::SFramedPacket framedPacket{};
					NetworkLib::Packet::View::FPacketView contentPacketView{};
					if (!tryReceiveNextContentPacket("chat-bootstrap", framedPacket, contentPacketView))
					{
						closesocket(clientSocket);
						clientSocket = INVALID_SOCKET;
						return sessionResult;
					}

					if (contentPacketView.opcode == Generated::Chat::FRoomSnapshotRp::kOpcode)
					{
						Generated::Chat::FRoomSnapshotRp snapshotResponse;
						if (!NetworkLib::Packet::Serialization::DeserializeContentPacket(contentPacketView, snapshotResponse))
						{
							sessionResult.errorMessage = "chat snapshot response deserialize failed.";
							closesocket(clientSocket);
							clientSocket = INVALID_SOCKET;
							return sessionResult;
						}

						if (snapshotResponse.roomId != roomId || snapshotResponse.participants.size() != 3)
						{
							sessionResult.errorMessage = "chat snapshot response validation failed.";
							closesocket(clientSocket);
							clientSocket = INVALID_SOCKET;
							return sessionResult;
						}

						receivedSnapshotResponse = true;
						if (options.bootstrapTrace)
						{
							std::cerr << "bootstrap trace: snapshot response ok. sessionIndex=" << sessionIndex
								<< " roomId=" << roomId << "\n";
						}
						continue;
					}

					if (contentPacketView.opcode == Generated::Chat::FRoomBinarySnapshotNoti::kOpcode)
					{
						Generated::Chat::FRoomBinarySnapshotNoti binarySnapshot;
						if (!NetworkLib::Packet::Serialization::DeserializeContentPacket(contentPacketView, binarySnapshot))
						{
							sessionResult.errorMessage = "chat binary snapshot deserialize failed.";
							closesocket(clientSocket);
							clientSocket = INVALID_SOCKET;
							return sessionResult;
						}

						if (!binarySnapshot.ContainsBorrowedViews())
						{
							sessionResult.errorMessage = "chat binary snapshot should report borrowed view payload.";
							closesocket(clientSocket);
							clientSocket = INVALID_SOCKET;
							return sessionResult;
						}

						const std::array<std::uint8_t, 8> expectedPayload = {
							static_cast<std::uint8_t>(roomId & 0xFF),
							static_cast<std::uint8_t>((roomId >> 8) & 0xFF),
							0x10, 0x20, 0x30, 0x40, 0x50, 0x60
						};

						const std::span<const std::uint8_t> binaryPayload = binarySnapshot.GetPayloadValue();
						if (binarySnapshot.roomId != roomId || binaryPayload.size() != expectedPayload.size())
						{
							sessionResult.errorMessage = "chat binary snapshot validation failed.";
							closesocket(clientSocket);
							clientSocket = INVALID_SOCKET;
							return sessionResult;
						}

						for (std::size_t payloadIndex = 0; payloadIndex < expectedPayload.size(); ++payloadIndex)
						{
							if (binaryPayload[payloadIndex] != expectedPayload[payloadIndex])
							{
								sessionResult.errorMessage = "chat binary snapshot payload mismatch.";
								closesocket(clientSocket);
								clientSocket = INVALID_SOCKET;
								return sessionResult;
							}
						}

						receivedBinarySnapshot = true;
						if (options.bootstrapTrace)
						{
							std::cerr << "bootstrap trace: binary snapshot ok. sessionIndex=" << sessionIndex
								<< " roomId=" << roomId << "\n";
						}
						continue;
					}

					std::ostringstream oss;
					oss << "unexpected pre-echo opcode: " << contentPacketView.opcode;
					sessionResult.errorMessage = oss.str();
					closesocket(clientSocket);
					clientSocket = INVALID_SOCKET;
					return sessionResult;
				}

				requiresBootstrapAfterConnect = false;
			}

			for (int requestIndex = 0; requestIndex < options.requestCount; ++requestIndex)
			{
				const std::string requestMessage = BuildRequestMessage(sessionIndex, requestSequence++, options.payloadSize);
				for (const std::string& expectedResponse : BuildExpectedResponseMessages(requestMessage, options))
				{
					++expectedResponseCounts[expectedResponse];
				}

				Generated::Echo::FEchoRq requestPacket;
				requestPacket.SetMessageValue(requestMessage);
				std::vector<char> serializedPayload = NetworkLib::Packet::Serialization::SerializeContentPacket(requestPacket);
				const std::uint8_t requestRandomKey = static_cast<std::uint8_t>((0x31 + requestSequence + sessionIndex) & 0xFF);
				packetCipher.Encode(serializedPayload.data(), static_cast<int>(serializedPayload.size()), requestRandomKey);

				NetworkLib::Packet::Framing::SOutgoingPacket outgoingPacket{};
				outgoingPacket.randomKey = requestRandomKey;
				outgoingPacket.checkSum =
					NetworkLib::Packet::Framing::CalculatePacketChecksum(
						serializedPayload.data(),
						static_cast<std::int32_t>(serializedPayload.size()));
				outgoingPacket.payload = serializedPayload.data();
				outgoingPacket.payloadLength = static_cast<std::int32_t>(serializedPayload.size());

				std::vector<char> outboundPacket;
				if (!packetFramer.BuildPacket(outgoingPacket, outboundPacket))
				{
					std::ostringstream oss;
					oss << "BuildPacket failed for request " << requestIndex << '.';
					sessionResult.errorMessage = oss.str();
					closesocket(clientSocket);
					clientSocket = INVALID_SOCKET;
					return sessionResult;
				}

				sendBatchBuffer.insert(sendBatchBuffer.end(), outboundPacket.begin(), outboundPacket.end());
				const bool shouldFlush =
					((requestIndex + 1) % options.packetsPerSend) == 0 ||
					requestIndex == options.requestCount - 1;
				if (!shouldFlush)
				{
					continue;
				}

				if (!SendPacketWithOptionalChunking(clientSocket, sendBatchBuffer, options.sendChunkSize, options.sendChunkDelayMs))
				{
					std::ostringstream oss;
					oss << "send failed for request " << requestIndex << ". error=" << WSAGetLastError();
					sessionResult.errorMessage = oss.str();
					closesocket(clientSocket);
					clientSocket = INVALID_SOCKET;
					return sessionResult;
				}
				sendBatchBuffer.clear();
			}

			const int expectedResponseCount = options.requestCount * options.responseThreadCount * options.responsesPerThread;
			int cycleReceivedResponseCount = 0;

			while (cycleReceivedResponseCount < expectedResponseCount)
			{
				NetworkLib::Packet::Framing::SFramedPacket framedPacket{};
				NetworkLib::Packet::View::FPacketView contentPacketView{};
				if (!tryReceiveNextContentPacket("echo-response", framedPacket, contentPacketView))
				{
					closesocket(clientSocket);
					clientSocket = INVALID_SOCKET;
					return sessionResult;
				}

				if (contentPacketView.opcode != Generated::Echo::FEchoRp::kOpcode)
				{
					std::ostringstream oss;
					oss << "unexpected opcode: " << contentPacketView.opcode;
					sessionResult.errorMessage = oss.str();
					closesocket(clientSocket);
					clientSocket = INVALID_SOCKET;
					return sessionResult;
				}

				Generated::Echo::FEchoRp responsePacket;
				if (!NetworkLib::Packet::Serialization::DeserializeContentPacket(contentPacketView, responsePacket))
				{
					sessionResult.errorMessage = "response packet deserialize failed.";
					closesocket(clientSocket);
					clientSocket = INVALID_SOCKET;
					return sessionResult;
				}

				if (!responsePacket.ContainsBorrowedViews())
				{
					sessionResult.errorMessage = "echo response should report borrowed view payload.";
					closesocket(clientSocket);
					clientSocket = INVALID_SOCKET;
					return sessionResult;
				}

				const std::string responseMessage(responsePacket.GetMessageValue());

				auto expectedIt = expectedResponseCounts.find(responseMessage);
				if (expectedIt == expectedResponseCounts.end() || expectedIt->second <= 0)
				{
					std::ostringstream oss;
					oss << "unexpected response=" << responseMessage;
					sessionResult.errorMessage = oss.str();
					closesocket(clientSocket);
					clientSocket = INVALID_SOCKET;
					return sessionResult;
				}
				--expectedIt->second;

				if (options.verbose)
				{
					std::cout << "session[" << sessionIndex << "] response[" << sessionResult.receivedResponseCount
						<< "]: " << responseMessage << "\n";
				}

				++cycleReceivedResponseCount;
				++sessionResult.receivedResponseCount;
			}

			for (const auto& [message, remainingCount] : expectedResponseCounts)
			{
				if (remainingCount != 0)
				{
					std::ostringstream oss;
					oss << "missing response=" << message << " remaining=" << remainingCount;
					sessionResult.errorMessage = oss.str();
					closesocket(clientSocket);
					clientSocket = INVALID_SOCKET;
					return sessionResult;
				}
			}

			if (options.holdSeconds <= 0)
			{
				break;
			}

			if (ShouldReconnect(options, randomEngine))
			{
				shutdown(clientSocket, SD_BOTH);
				closesocket(clientSocket);
				clientSocket = INVALID_SOCKET;
				requiresBootstrapAfterConnect = true;
				if (options.reconnectDelayMs > 0)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(options.reconnectDelayMs));
				}
			}

			if (std::chrono::steady_clock::now() >= deadline)
			{
				break;
			}

			if (options.intervalMs > 0)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(options.intervalMs));
			}
		}

		if (clientSocket != INVALID_SOCKET)
		{
			shutdown(clientSocket, SD_BOTH);
			closesocket(clientSocket);
		}
		sessionResult.succeeded = true;
		return sessionResult;
	}
}

int main(int argc, char* argv[])
{
	SClientOptions options{};
	if (!ParseArguments(argc, argv, options))
	{
		std::cerr << "usage: EchoClient.exe [--server-ip 127.0.0.1] [--port 19000] [--login-userid-base 1000] [--sessions 1] [--count 10] [--payload-size 64] [--send-chunk-size 8] [--send-chunk-delay-ms 1] [--recv-buffer-size 16] [--response-thread-count 1] [--responses-per-thread 1] [--hold-seconds 0] [--interval-ms 1000] [--packets-per-send 1] [--reconnect-probability-percent 0] [--reconnect-delay-ms 100] [--recv-timeout-ms 0] [--disable-page-pool] [--page-size 4096] [--bootstrap-trace] [--quiet]\n";
		return 1;
	}

	NetworkLib::Packet::Buffer::FPacketBuffer::ConfigurePageReuse(
		options.enablePagePool,
		static_cast<std::size_t>(options.pageSize));

	WSADATA wsaData{};
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		std::cerr << "WSAStartup failed.\n";
		return 1;
	}

	std::vector<SSessionResult> sessionResults(static_cast<std::size_t>(options.sessionCount));
	std::vector<std::thread> sessionThreads;
	sessionThreads.reserve(static_cast<std::size_t>(options.sessionCount));

	for (int sessionIndex = 0; sessionIndex < options.sessionCount; ++sessionIndex)
	{
		sessionThreads.emplace_back([&, sessionIndex]()
		{
			sessionResults[static_cast<std::size_t>(sessionIndex)] = RunSingleSession(sessionIndex, options);
		});
	}

	for (std::thread& sessionThread : sessionThreads)
	{
		sessionThread.join();
	}

	WSACleanup();

	int totalResponses = 0;
	int successCount = 0;
	for (int sessionIndex = 0; sessionIndex < options.sessionCount; ++sessionIndex)
	{
		const SSessionResult& sessionResult = sessionResults[static_cast<std::size_t>(sessionIndex)];
		totalResponses += sessionResult.receivedResponseCount;
		if (!sessionResult.succeeded)
		{
			std::cerr << "session[" << sessionIndex << "] failed: " << sessionResult.errorMessage << "\n";
			return 1;
		}

		++successCount;
	}

	std::cout << "echo validation succeeded. sessions=" << successCount
		<< " responses=" << totalResponses
		<< " payloadSize=" << options.payloadSize
		<< " sendChunkSize=" << options.sendChunkSize
		<< " recvBufferSize=" << options.recvBufferSize
		<< " responseThreadCount=" << options.responseThreadCount
		<< " responsesPerThread=" << options.responsesPerThread
		<< " intervalMs=" << options.intervalMs
		<< " packetsPerSend=" << options.packetsPerSend
		<< " reconnectProbabilityPercent=" << options.reconnectProbabilityPercent
		<< " holdSeconds=" << options.holdSeconds << "\n";
	return 0;
}
