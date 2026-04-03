#include "Pch.h"

#include "Foundation/Diagnostics/Rtt/FRttCsvLogger.h"
#include "Foundation/Diagnostics/Rtt/FRttMetricsRuntime.h"
#include "Foundation/Diagnostics/Rtt/FRttThreadLocalCollector.h"
#include "Crypto/FDefaultPacketCipher.h"
#include "EchoServer/Contents/Room/RoomFlowTypes.h"
#include "Generated/Packets/Chat/ChatPackets.h"
#include "Generated/Packets/Echo/EchoPackets.h"
#include "Generated/Packets/Login/LoginPackets.h"
#include "Packet/Buffer/FPacketBuffer.h"
#include "Packet/Framing/FDefaultPacketFramer.h"
#include "Packet/Framing/PacketTypes.h"
#include "Packet/Serialization/FPacketSerialization.h"
#include "Packet/View/FPacketView.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <optional>
#include <random>
#include <thread>
#include <unordered_map>
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
		int roomListRecvTimeoutMs = -1;
		int echoRecvTimeoutMs = -1;
		std::string rttCsvPath;
		int rttFlushIntervalSeconds = 60;
		int roomChangeProbabilityPercent = 25;
		int maxRoomEnterRetryCount = 5;
		int maxRoomChangeRetryCount = 3;
		int traceSessionIndex = 0;
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

	struct SRoomCandidate
	{
		std::uint32_t roomId = 0;
		std::string roomName;
		std::uint32_t participantCount = 0;
		std::uint32_t capacity = 0;
		bool joinable = false;
	};

	enum class ERttStage : std::uint8_t
	{
		LoginResponse = 0,
		RoomList,
		RoomEnter,
		EchoResponse,
		RoomChangeList,
		RoomChange,
		Count
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
				if (!TryParseInt(argv[++argumentIndex], outOptions.reconnectProbabilityPercent) ||
					outOptions.reconnectProbabilityPercent < 0 ||
					outOptions.reconnectProbabilityPercent > 100)
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
			else if (argument == "--room-list-recv-timeout-ms" && argumentIndex + 1 < argc)
			{
				if (!TryParseInt(argv[++argumentIndex], outOptions.roomListRecvTimeoutMs) || outOptions.roomListRecvTimeoutMs < 0)
				{
					return false;
				}
			}
			else if (argument == "--echo-recv-timeout-ms" && argumentIndex + 1 < argc)
			{
				if (!TryParseInt(argv[++argumentIndex], outOptions.echoRecvTimeoutMs) || outOptions.echoRecvTimeoutMs < 0)
				{
					return false;
				}
			}
			else if (argument == "--rtt-csv-path" && argumentIndex + 1 < argc)
			{
				outOptions.rttCsvPath = argv[++argumentIndex];
			}
			else if (argument == "--rtt-flush-interval-seconds" && argumentIndex + 1 < argc)
			{
				if (!TryParseInt(argv[++argumentIndex], outOptions.rttFlushIntervalSeconds) ||
					outOptions.rttFlushIntervalSeconds <= 0)
				{
					return false;
				}
			}
			else if (argument == "--room-change-probability-percent" && argumentIndex + 1 < argc)
			{
				if (!TryParseInt(argv[++argumentIndex], outOptions.roomChangeProbabilityPercent) ||
					outOptions.roomChangeProbabilityPercent < 0 ||
					outOptions.roomChangeProbabilityPercent > 100)
				{
					return false;
				}
			}
			else if (argument == "--max-room-enter-retries" && argumentIndex + 1 < argc)
			{
				if (!TryParseInt(argv[++argumentIndex], outOptions.maxRoomEnterRetryCount) ||
					outOptions.maxRoomEnterRetryCount <= 0)
				{
					return false;
				}
			}
			else if (argument == "--max-room-change-retries" && argumentIndex + 1 < argc)
			{
				if (!TryParseInt(argv[++argumentIndex], outOptions.maxRoomChangeRetryCount) ||
					outOptions.maxRoomChangeRetryCount <= 0)
				{
					return false;
				}
			}
			else if (argument == "--trace-session-index" && argumentIndex + 1 < argc)
			{
				if (!TryParseInt(argv[++argumentIndex], outOptions.traceSessionIndex) ||
					outOptions.traceSessionIndex < 0)
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

	int ResolveStageRecvTimeoutMs(const SClientOptions& options, const char* stageName)
	{
		if (stageName != nullptr)
		{
			const std::string_view stage(stageName);
			if ((stage == "room-list" || stage == "room-change-list") && options.roomListRecvTimeoutMs >= 0)
			{
				return options.roomListRecvTimeoutMs;
			}

			if (stage == "echo-response" && options.echoRecvTimeoutMs >= 0)
			{
				return options.echoRecvTimeoutMs;
			}
		}

		return options.recvTimeoutMs;
	}

	Foundation::Diagnostics::FRttStageIndex ToRttStageIndex(const ERttStage stage)
	{
		return static_cast<Foundation::Diagnostics::FRttStageIndex>(stage);
	}

	Foundation::Diagnostics::SRttMetricsConfig BuildRttMetricsConfig(const SClientOptions& options)
	{
		Foundation::Diagnostics::SRttMetricsConfig config{};
		config.flushIntervalSeconds = options.rttFlushIntervalSeconds;
		config.stageNames =
		{
			"login-response",
			"room-list",
			"room-enter",
			"echo-response",
			"room-change-list",
			"room-change"
		};
		return config;
	}

	using FRttMetricsRuntime = Foundation::Diagnostics::FRttMetricsRuntime;
	using FRttThreadLocalCollector = Foundation::Diagnostics::FRttThreadLocalCollector;
	using FRttCsvLogger = Foundation::Diagnostics::FRttCsvLogger;
	using SRttPendingRequest = Foundation::Diagnostics::SRttPendingRequest;

	bool WaitUntilSocketReadable(SOCKET clientSocket, int timeoutMs)
	{
		if (timeoutMs <= 0)
		{
			return true;
		}

		fd_set readSet;
		FD_ZERO(&readSet);
		FD_SET(clientSocket, &readSet);

		timeval timeout{};
		timeout.tv_sec = timeoutMs / 1000;
		timeout.tv_usec = static_cast<long>((timeoutMs % 1000) * 1000);
		const int selectResult = select(0, &readSet, nullptr, nullptr, &timeout);
		return selectResult > 0 && FD_ISSET(clientSocket, &readSet);
	}

	std::string BuildRequestMessage(int sessionIndex, int requestIndex, int payloadSize)
	{
		std::ostringstream messageBuilder;
		messageBuilder << "echo-s" << sessionIndex << "-r" << requestIndex;
		std::string message = messageBuilder.str();
		if (payloadSize <= static_cast<int>(message.size()))
		{
			// Even with a small payload budget, keep the per-session request key unique.
			std::ostringstream compactBuilder;
			compactBuilder << 'e'
				<< std::uppercase << std::hex << std::setw(std::max(1, payloadSize - 1)) << std::setfill('0')
				<< static_cast<std::uint32_t>(requestIndex);
			message = compactBuilder.str();
			if (static_cast<int>(message.size()) > payloadSize)
			{
				message = message.substr(static_cast<std::size_t>(message.size() - payloadSize));
			}
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

	bool ShouldAttemptRoomChange(const SClientOptions& options, std::mt19937& randomEngine)
	{
		if (options.roomChangeProbabilityPercent <= 0)
		{
			return false;
		}

		std::uniform_int_distribution<int> distribution(1, 100);
		return distribution(randomEngine) <= options.roomChangeProbabilityPercent;
	}

	void TraceSession(const SClientOptions& options, int sessionIndex, const std::string& message)
	{
		if (!options.bootstrapTrace || sessionIndex != options.traceSessionIndex)
		{
			return;
		}

		std::cerr << "[trace][session " << sessionIndex << "] " << message << "\n";
	}

	bool SendContentPacketRequest(
		SOCKET clientSocket,
		NetworkLib::Crypto::FDefaultPacketCipher& packetCipher,
		NetworkLib::Packet::Framing::FDefaultPacketFramer& packetFramer,
		const NetworkLib::Packet::Serialization::IContentPacket& packet,
		const std::uint8_t randomKey,
		const SClientOptions& options,
		std::string& outErrorMessage)
	{
		std::vector<char> serializedPayload = NetworkLib::Packet::Serialization::SerializeContentPacket(packet);
		packetCipher.Encode(serializedPayload.data(), static_cast<int>(serializedPayload.size()), randomKey);

		NetworkLib::Packet::Framing::SOutgoingPacket outgoingPacket{};
		outgoingPacket.randomKey = randomKey;
		outgoingPacket.checkSum =
			NetworkLib::Packet::Framing::CalculatePacketChecksum(
				serializedPayload.data(),
				static_cast<std::int32_t>(serializedPayload.size()));
		outgoingPacket.payload = serializedPayload.data();
		outgoingPacket.payloadLength = static_cast<std::int32_t>(serializedPayload.size());

		std::vector<char> outboundPacket;
		if (!packetFramer.BuildPacket(outgoingPacket, outboundPacket))
		{
			outErrorMessage = "BuildPacket failed.";
			return false;
		}

		if (!SendPacketWithOptionalChunking(clientSocket, outboundPacket, options.sendChunkSize, options.sendChunkDelayMs))
		{
			std::ostringstream oss;
			oss << "send failed. error=" << WSAGetLastError();
			outErrorMessage = oss.str();
			return false;
		}

		return true;
	}

	bool TryBuildRoomCandidates(const Generated::Chat::FRoomListRp& responsePacket, std::vector<SRoomCandidate>& outCandidates)
	{
		const std::size_t roomCount = responsePacket.roomIds.size();
		if (responsePacket.roomNames.size() != roomCount ||
			responsePacket.participantCounts.size() != roomCount ||
			responsePacket.capacities.size() != roomCount ||
			responsePacket.joinableFlags.size() != roomCount)
		{
			return false;
		}

		outCandidates.clear();
		outCandidates.reserve(roomCount);
		for (std::size_t index = 0; index < roomCount; ++index)
		{
			outCandidates.push_back({
				responsePacket.roomIds[index],
				responsePacket.roomNames[index],
				responsePacket.participantCounts[index],
				responsePacket.capacities[index],
				responsePacket.joinableFlags[index] != 0
			});
		}

		return true;
	}

	std::vector<SRoomCandidate> BuildJoinableRoomCandidates(
		const std::vector<SRoomCandidate>& roomCandidates,
		const std::optional<std::uint32_t> excludedRoomId = std::nullopt)
	{
		std::vector<SRoomCandidate> joinableRooms;
		for (const SRoomCandidate& roomCandidate : roomCandidates)
		{
			if (!roomCandidate.joinable)
			{
				continue;
			}

			if (excludedRoomId.has_value() && roomCandidate.roomId == excludedRoomId.value())
			{
				continue;
			}

			joinableRooms.push_back(roomCandidate);
		}

		return joinableRooms;
	}

	std::optional<SRoomCandidate> PickRandomRoomCandidate(
		const std::vector<SRoomCandidate>& roomCandidates,
		std::mt19937& randomEngine)
	{
		if (roomCandidates.empty())
		{
			return std::nullopt;
		}

		std::uniform_int_distribution<std::size_t> distribution(0, roomCandidates.size() - 1);
		return roomCandidates[distribution(randomEngine)];
	}

	SSessionResult RunSingleSession(int sessionIndex, const SClientOptions& options, FRttMetricsRuntime* rttMetricsRuntime)
	{
		SSessionResult sessionResult{};
		FRttThreadLocalCollector rttCollector(rttMetricsRuntime);
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
		std::optional<std::uint32_t> currentRoomId;

		while (true)
		{
			if (clientSocket == INVALID_SOCKET)
			{
				if (!TryConnectSocket(options, clientSocket, sessionResult.errorMessage))
				{
					return sessionResult;
				}

				requiresBootstrapAfterConnect = true;
				currentRoomId.reset();
			}

			std::vector<char> inboundBuffer;
			inboundBuffer.reserve(static_cast<std::size_t>(options.recvBufferSize) * 2);
			std::vector<char> recvChunk(static_cast<std::size_t>(options.recvBufferSize));
			std::unordered_map<std::string, int> expectedResponseCounts;
			std::unordered_map<std::string, SRttPendingRequest> expectedResponseMetrics;
			std::vector<char> sendBatchBuffer;
			auto tryReceiveNextContentPacket =
				[&](
					const char* stageName,
					NetworkLib::Packet::Framing::SFramedPacket& outFramedPacket,
					NetworkLib::Packet::View::FPacketView& outContentPacketView,
					const std::optional<ERttStage> timeoutStage = std::nullopt,
					const SRttPendingRequest* successPendingRequest = nullptr) -> bool
				{
					const int effectiveRecvTimeoutMs = ResolveStageRecvTimeoutMs(options, stageName);
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

							packetCipher.Decode(
								outFramedPacket.payload.data(),
								static_cast<int>(outFramedPacket.payload.size()),
								outFramedPacket.randomKey);

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

							if (successPendingRequest != nullptr)
							{
								rttCollector.RecordSample(*successPendingRequest, std::chrono::system_clock::now());
							}

							return true;
						}

						if (!WaitUntilSocketReadable(clientSocket, effectiveRecvTimeoutMs))
						{
							if (timeoutStage.has_value())
							{
								rttCollector.RecordTimeout(ToRttStageIndex(*timeoutStage), std::chrono::system_clock::now());
							}

							std::ostringstream oss;
							oss << "recv failed at stage="
								<< (stageName != nullptr ? stageName : "unknown")
								<< " sessionIndex=" << sessionIndex
								<< " error=" << WSAETIMEDOUT
								<< " (timeout)";
							sessionResult.errorMessage = oss.str();
							return false;
						}

						const int recvBytes = recv(clientSocket, recvChunk.data(), static_cast<int>(recvChunk.size()), 0);
						if (recvBytes <= 0)
						{
							const int errorCode = WSAGetLastError();
							if (errorCode == WSAETIMEDOUT && timeoutStage.has_value())
							{
								rttCollector.RecordTimeout(ToRttStageIndex(*timeoutStage), std::chrono::system_clock::now());
							}
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
				if (!SendContentPacketRequest(
					clientSocket,
					packetCipher,
					packetFramer,
					loginRequest,
					static_cast<std::uint8_t>((0x21 + sessionIndex) & 0xFF),
					options,
					sessionResult.errorMessage))
				{
					closesocket(clientSocket);
					clientSocket = INVALID_SOCKET;
					return sessionResult;
				}

				const SRttPendingRequest loginPendingRequest =
					rttCollector.BeginRequest(ToRttStageIndex(ERttStage::LoginResponse), sessionIndex);
				NetworkLib::Packet::Framing::SFramedPacket loginResponseFramedPacket{};
				NetworkLib::Packet::View::FPacketView loginResponsePacketView{};
				if (!tryReceiveNextContentPacket(
					"login-response",
					loginResponseFramedPacket,
					loginResponsePacketView,
					ERttStage::LoginResponse,
					&loginPendingRequest))
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

				bool roomEntered = false;
				for (int attemptIndex = 0; attemptIndex < options.maxRoomEnterRetryCount && !roomEntered; ++attemptIndex)
				{
					Generated::Chat::FRoomListRq roomListRequest;
					if (!SendContentPacketRequest(
						clientSocket,
						packetCipher,
						packetFramer,
						roomListRequest,
						static_cast<std::uint8_t>((0x41 + sessionIndex + attemptIndex) & 0xFF),
						options,
						sessionResult.errorMessage))
					{
						closesocket(clientSocket);
						clientSocket = INVALID_SOCKET;
						return sessionResult;
					}

					const SRttPendingRequest roomListPendingRequest =
						rttCollector.BeginRequest(ToRttStageIndex(ERttStage::RoomList), sessionIndex);
					NetworkLib::Packet::Framing::SFramedPacket roomListFramedPacket{};
					NetworkLib::Packet::View::FPacketView roomListPacketView{};
					if (!tryReceiveNextContentPacket(
						"room-list",
						roomListFramedPacket,
						roomListPacketView,
						ERttStage::RoomList,
						&roomListPendingRequest))
					{
						closesocket(clientSocket);
						clientSocket = INVALID_SOCKET;
						return sessionResult;
					}

					if (roomListPacketView.opcode != Generated::Chat::FRoomListRp::kOpcode)
					{
						std::ostringstream oss;
						oss << "unexpected room list response opcode: " << roomListPacketView.opcode;
						sessionResult.errorMessage = oss.str();
						closesocket(clientSocket);
						clientSocket = INVALID_SOCKET;
						return sessionResult;
					}

					Generated::Chat::FRoomListRp roomListResponse;
					if (!NetworkLib::Packet::Serialization::DeserializeContentPacket(roomListPacketView, roomListResponse))
					{
						sessionResult.errorMessage = "room list response deserialize failed.";
						closesocket(clientSocket);
						clientSocket = INVALID_SOCKET;
						return sessionResult;
					}

					std::vector<SRoomCandidate> roomCandidates;
					if (!TryBuildRoomCandidates(roomListResponse, roomCandidates))
					{
						sessionResult.errorMessage = "room list response validation failed.";
						closesocket(clientSocket);
						clientSocket = INVALID_SOCKET;
						return sessionResult;
					}

					const auto joinableRooms = BuildJoinableRoomCandidates(roomCandidates);
					const auto targetRoomCandidate = PickRandomRoomCandidate(joinableRooms, randomEngine);
					if (!targetRoomCandidate.has_value())
					{
						sessionResult.errorMessage = "no joinable room available.";
						closesocket(clientSocket);
						clientSocket = INVALID_SOCKET;
						return sessionResult;
					}

					Generated::Chat::FRoomEnterRq roomEnterRequest;
					roomEnterRequest.roomId = targetRoomCandidate->roomId;
					TraceSession(options, sessionIndex, "send RoomEnterRq roomId=" + std::to_string(roomEnterRequest.roomId));
					if (!SendContentPacketRequest(
						clientSocket,
						packetCipher,
						packetFramer,
						roomEnterRequest,
						static_cast<std::uint8_t>((0x51 + sessionIndex + attemptIndex) & 0xFF),
						options,
						sessionResult.errorMessage))
					{
						closesocket(clientSocket);
						clientSocket = INVALID_SOCKET;
						return sessionResult;
					}

					const SRttPendingRequest roomEnterPendingRequest =
						rttCollector.BeginRequest(ToRttStageIndex(ERttStage::RoomEnter), sessionIndex);
					NetworkLib::Packet::Framing::SFramedPacket roomEnterFramedPacket{};
					NetworkLib::Packet::View::FPacketView roomEnterPacketView{};
					if (!tryReceiveNextContentPacket(
						"room-enter",
						roomEnterFramedPacket,
						roomEnterPacketView,
						ERttStage::RoomEnter,
						&roomEnterPendingRequest))
					{
						closesocket(clientSocket);
						clientSocket = INVALID_SOCKET;
						return sessionResult;
					}

					if (roomEnterPacketView.opcode != Generated::Chat::FRoomEnterRp::kOpcode)
					{
						std::ostringstream oss;
						oss << "unexpected room enter response opcode: " << roomEnterPacketView.opcode;
						sessionResult.errorMessage = oss.str();
						closesocket(clientSocket);
						clientSocket = INVALID_SOCKET;
						return sessionResult;
					}

					Generated::Chat::FRoomEnterRp roomEnterResponse;
					if (!NetworkLib::Packet::Serialization::DeserializeContentPacket(roomEnterPacketView, roomEnterResponse))
					{
						sessionResult.errorMessage = "room enter response deserialize failed.";
						closesocket(clientSocket);
						clientSocket = INVALID_SOCKET;
						return sessionResult;
					}

					if (roomEnterResponse.success)
					{
						currentRoomId = roomEnterResponse.roomId;
						TraceSession(options, sessionIndex, "recv RoomEnterRp success roomId=" + std::to_string(roomEnterResponse.roomId));
						roomEntered = true;
						if (options.bootstrapTrace)
						{
							std::cerr << "bootstrap trace: room enter ok. sessionIndex=" << sessionIndex
								<< " roomId=" << roomEnterResponse.roomId << "\n";
						}
						break;
					}

					const auto resultCode = static_cast<EchoServer::Contents::ERoomFlowResultCode>(roomEnterResponse.resultCode);
					if (!EchoServer::Contents::IsNormalRoomFlowFailure(resultCode))
					{
						std::ostringstream oss;
						oss << "room enter failed with abnormal resultCode="
							<< EchoServer::Contents::ToString(resultCode);
						sessionResult.errorMessage = oss.str();
						closesocket(clientSocket);
						clientSocket = INVALID_SOCKET;
						return sessionResult;
					}
				}

				if (!roomEntered)
				{
					sessionResult.errorMessage = "room enter retry exhausted.";
					closesocket(clientSocket);
					clientSocket = INVALID_SOCKET;
					return sessionResult;
				}

				requiresBootstrapAfterConnect = false;
			}

			std::vector<std::string> pendingBatchResponseMetrics;
			for (int requestIndex = 0; requestIndex < options.requestCount; ++requestIndex)
				{
				const std::string requestMessage = BuildRequestMessage(sessionIndex, requestSequence++, options.payloadSize);
				const std::vector<std::string> expectedResponses = BuildExpectedResponseMessages(requestMessage, options);
				for (const std::string& expectedResponse : expectedResponses)
				{
					++expectedResponseCounts[expectedResponse];
					pendingBatchResponseMetrics.push_back(expectedResponse);
				}

				TraceSession(
					options,
					sessionIndex,
					"sent echo batch requestCount=" + std::to_string(options.requestCount) +
						" currentRoomId=" + (currentRoomId.has_value() ? std::to_string(*currentRoomId) : std::string("none")));

				Generated::Echo::FEchoRq requestPacket;
				requestPacket.SetMessageValue(requestMessage);
				std::vector<char> serializedPayload = NetworkLib::Packet::Serialization::SerializeContentPacket(requestPacket);
				const std::uint8_t requestRandomKey = static_cast<std::uint8_t>((0x61 + requestSequence + sessionIndex) & 0xFF);
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

				const auto batchSentSteady = std::chrono::steady_clock::now();
				const auto batchSentSystem = std::chrono::system_clock::now();
				for (const std::string& expectedResponse : pendingBatchResponseMetrics)
				{
					SRttPendingRequest pendingRequest{};
					pendingRequest.stageIndex = ToRttStageIndex(ERttStage::EchoResponse);
					pendingRequest.sessionIndex = sessionIndex;
					pendingRequest.sentSteady = batchSentSteady;
					pendingRequest.sentSystem = batchSentSystem;
					expectedResponseMetrics.insert_or_assign(expectedResponse, pendingRequest);
				}
				pendingBatchResponseMetrics.clear();
			}

			const int expectedResponseCount = options.requestCount * options.responseThreadCount * options.responsesPerThread;
			int cycleReceivedResponseCount = 0;

			while (cycleReceivedResponseCount < expectedResponseCount)
			{
				NetworkLib::Packet::Framing::SFramedPacket framedPacket{};
				NetworkLib::Packet::View::FPacketView contentPacketView{};
				if (!tryReceiveNextContentPacket(
					"echo-response",
					framedPacket,
					contentPacketView,
					ERttStage::EchoResponse))
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
				TraceSession(
					options,
					sessionIndex,
					"recv EchoRp message=" + responseMessage +
						" currentRoomId=" + (currentRoomId.has_value() ? std::to_string(*currentRoomId) : std::string("none")));
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

				auto pendingMetricIt = expectedResponseMetrics.find(responseMessage);
				if (pendingMetricIt != expectedResponseMetrics.end())
				{
					rttCollector.RecordSample(pendingMetricIt->second, std::chrono::system_clock::now());
					expectedResponseMetrics.erase(pendingMetricIt);
				}

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

			if (currentRoomId.has_value() && ShouldAttemptRoomChange(options, randomEngine))
			{
				for (int attemptIndex = 0; attemptIndex < options.maxRoomChangeRetryCount; ++attemptIndex)
				{
					Generated::Chat::FRoomListRq roomListRequest;
					if (!SendContentPacketRequest(
						clientSocket,
						packetCipher,
						packetFramer,
						roomListRequest,
						static_cast<std::uint8_t>((0x71 + sessionIndex + attemptIndex) & 0xFF),
						options,
						sessionResult.errorMessage))
					{
						closesocket(clientSocket);
						clientSocket = INVALID_SOCKET;
						return sessionResult;
					}

					const SRttPendingRequest roomChangeListPendingRequest =
						rttCollector.BeginRequest(ToRttStageIndex(ERttStage::RoomChangeList), sessionIndex);
					NetworkLib::Packet::Framing::SFramedPacket roomListFramedPacket{};
					NetworkLib::Packet::View::FPacketView roomListPacketView{};
					if (!tryReceiveNextContentPacket(
						"room-change-list",
						roomListFramedPacket,
						roomListPacketView,
						ERttStage::RoomChangeList,
						&roomChangeListPendingRequest))
					{
						closesocket(clientSocket);
						clientSocket = INVALID_SOCKET;
						return sessionResult;
					}

					if (roomListPacketView.opcode != Generated::Chat::FRoomListRp::kOpcode)
					{
						std::ostringstream oss;
						oss << "unexpected room list response opcode during change: " << roomListPacketView.opcode;
						sessionResult.errorMessage = oss.str();
						closesocket(clientSocket);
						clientSocket = INVALID_SOCKET;
						return sessionResult;
					}

					Generated::Chat::FRoomListRp roomListResponse;
					if (!NetworkLib::Packet::Serialization::DeserializeContentPacket(roomListPacketView, roomListResponse))
					{
						sessionResult.errorMessage = "room list response deserialize failed during change.";
						closesocket(clientSocket);
						clientSocket = INVALID_SOCKET;
						return sessionResult;
					}

					std::vector<SRoomCandidate> roomCandidates;
					if (!TryBuildRoomCandidates(roomListResponse, roomCandidates))
					{
						sessionResult.errorMessage = "room list response validation failed during change.";
						closesocket(clientSocket);
						clientSocket = INVALID_SOCKET;
						return sessionResult;
					}

					const auto joinableRooms = BuildJoinableRoomCandidates(roomCandidates, currentRoomId);
					const auto targetRoomCandidate = PickRandomRoomCandidate(joinableRooms, randomEngine);
					if (!targetRoomCandidate.has_value())
					{
						break;
					}

					Generated::Chat::FRoomChangeRq roomChangeRequest;
					roomChangeRequest.targetRoomId = targetRoomCandidate->roomId;
					TraceSession(
						options,
						sessionIndex,
						"send RoomChangeRq fromRoomId=" + (currentRoomId.has_value() ? std::to_string(*currentRoomId) : std::string("none")) +
							" targetRoomId=" + std::to_string(roomChangeRequest.targetRoomId));
					if (!SendContentPacketRequest(
						clientSocket,
						packetCipher,
						packetFramer,
						roomChangeRequest,
						static_cast<std::uint8_t>((0x81 + sessionIndex + attemptIndex) & 0xFF),
						options,
						sessionResult.errorMessage))
					{
						closesocket(clientSocket);
						clientSocket = INVALID_SOCKET;
						return sessionResult;
					}

					const SRttPendingRequest roomChangePendingRequest =
						rttCollector.BeginRequest(ToRttStageIndex(ERttStage::RoomChange), sessionIndex);
					NetworkLib::Packet::Framing::SFramedPacket roomChangeFramedPacket{};
					NetworkLib::Packet::View::FPacketView roomChangePacketView{};
					if (!tryReceiveNextContentPacket(
						"room-change",
						roomChangeFramedPacket,
						roomChangePacketView,
						ERttStage::RoomChange,
						&roomChangePendingRequest))
					{
						closesocket(clientSocket);
						clientSocket = INVALID_SOCKET;
						return sessionResult;
					}

					if (roomChangePacketView.opcode != Generated::Chat::FRoomChangeRp::kOpcode)
					{
						std::ostringstream oss;
						oss << "unexpected room change response opcode: " << roomChangePacketView.opcode;
						sessionResult.errorMessage = oss.str();
						closesocket(clientSocket);
						clientSocket = INVALID_SOCKET;
						return sessionResult;
					}

					Generated::Chat::FRoomChangeRp roomChangeResponse;
					if (!NetworkLib::Packet::Serialization::DeserializeContentPacket(roomChangePacketView, roomChangeResponse))
					{
						sessionResult.errorMessage = "room change response deserialize failed.";
						closesocket(clientSocket);
						clientSocket = INVALID_SOCKET;
						return sessionResult;
					}

					if (roomChangeResponse.success)
					{
						TraceSession(
							options,
							sessionIndex,
							"recv RoomChangeRp success previousRoomId=" + std::to_string(roomChangeResponse.previousRoomId) +
								" currentRoomId=" + std::to_string(roomChangeResponse.currentRoomId));
						currentRoomId = roomChangeResponse.currentRoomId;
						break;
					}

					const auto resultCode = static_cast<EchoServer::Contents::ERoomFlowResultCode>(roomChangeResponse.resultCode);
					TraceSession(
						options,
						sessionIndex,
						"recv RoomChangeRp failure resultCode=" + std::string(EchoServer::Contents::ToString(resultCode)));
					if (!EchoServer::Contents::IsNormalRoomFlowFailure(resultCode))
					{
						std::ostringstream oss;
						oss << "room change failed with abnormal resultCode="
							<< EchoServer::Contents::ToString(resultCode);
						sessionResult.errorMessage = oss.str();
						closesocket(clientSocket);
						clientSocket = INVALID_SOCKET;
						return sessionResult;
					}
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
				currentRoomId.reset();
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
		std::cerr
			<< "usage: EchoClient.exe [--server-ip 127.0.0.1] [--port 19000] [--login-userid-base 1000] "
			<< "[--sessions 1] [--count 10] [--payload-size 64] [--send-chunk-size 8] [--send-chunk-delay-ms 1] "
			<< "[--recv-buffer-size 16] [--response-thread-count 1] [--responses-per-thread 1] [--hold-seconds 0] "
			<< "[--interval-ms 1000] [--packets-per-send 1] [--reconnect-probability-percent 0] [--reconnect-delay-ms 100] "
			<< "[--recv-timeout-ms 0] [--room-list-recv-timeout-ms -1] [--echo-recv-timeout-ms -1] "
			<< "[--rtt-csv-path path] [--rtt-flush-interval-seconds 60] "
			<< "[--room-change-probability-percent 25] [--max-room-enter-retries 5] "
			<< "[--max-room-change-retries 3] [--disable-page-pool] [--page-size 4096] [--bootstrap-trace] [--quiet]\n";
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

	std::unique_ptr<FRttMetricsRuntime> rttMetricsRuntime;
	std::unique_ptr<FRttCsvLogger> rttCsvLogger;
	if (!options.rttCsvPath.empty())
	{
		rttMetricsRuntime = std::make_unique<FRttMetricsRuntime>(BuildRttMetricsConfig(options));
		rttCsvLogger = std::make_unique<FRttCsvLogger>(*rttMetricsRuntime, options.rttCsvPath);
		rttCsvLogger->Start();
	}

	std::vector<SSessionResult> sessionResults(static_cast<std::size_t>(options.sessionCount));
	std::vector<std::thread> sessionThreads;
	sessionThreads.reserve(static_cast<std::size_t>(options.sessionCount));

	for (int sessionIndex = 0; sessionIndex < options.sessionCount; ++sessionIndex)
	{
		sessionThreads.emplace_back([&, sessionIndex]()
		{
			sessionResults[static_cast<std::size_t>(sessionIndex)] =
				RunSingleSession(sessionIndex, options, rttMetricsRuntime.get());
		});
	}

	for (std::thread& sessionThread : sessionThreads)
	{
		sessionThread.join();
	}

	if (rttCsvLogger)
	{
		rttCsvLogger->Stop();
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
		<< " roomChangeProbabilityPercent=" << options.roomChangeProbabilityPercent
		<< " holdSeconds=" << options.holdSeconds << "\n";
	return 0;
}
