#include "pch.h"

namespace TestClient
{
	namespace Config
	{
		inline std::string serverIp = "127.0.0.1";
		inline USHORT serverPort = 10611;
		inline BYTE packetCode = 0x77;
		inline INT parallelClientCount = 1;
		inline INT repeatCount = 1;
		inline INT64 accountNoBase = 10000;
		inline INT loginVersion = 1;
		inline std::string sessionKey = "TEST_SESSION_KEY";
		inline INT holdSeconds = 10;
		inline INT heartbeatIntervalMs = 5000;
		inline bool waitOnExit = true;
	}

	namespace Detail
	{
		struct NetPacket
		{
			NetworkLib::Core::Utils::Net::Header header{};
			std::vector<char> payload;
		};

		void AppendBytes(std::vector<char>& buffer, const void* src, size_t size)
		{
			const char* begin = static_cast<const char*>(src);
			buffer.insert(buffer.end(), begin, begin + size);
		}

		template <typename TValue>
		void AppendValue(std::vector<char>& buffer, const TValue& value)
		{
			AppendBytes(buffer, &value, sizeof(TValue));
		}

		bool SendAll(SOCKET socket, const char* data, int length)
		{
			int sentBytes = 0;
			while (sentBytes < length)
			{
				const int currentSent = send(socket, data + sentBytes, length - sentBytes, 0);
				if (currentSent == SOCKET_ERROR)
				{
					return false;
				}

				sentBytes += currentSent;
			}

			return true;
		}

		bool RecvAll(SOCKET socket, char* buffer, int length)
		{
			int receivedBytes = 0;
			while (receivedBytes < length)
			{
				const int currentReceived = recv(socket, buffer + receivedBytes, length - receivedBytes, 0);
				if (currentReceived <= 0)
				{
					return false;
				}

				receivedBytes += currentReceived;
			}

			return true;
		}

		std::filesystem::path FindConfigPath()
		{
			const std::array<std::filesystem::path, 5> candidates =
			{
				std::filesystem::path(L"EchoServer\\ServerConfig.conf"),
				std::filesystem::path(L"..\\EchoServer\\ServerConfig.conf"),
				std::filesystem::path(L"..\\..\\EchoServer\\ServerConfig.conf"),
				std::filesystem::path(L"..\\..\\..\\EchoServer\\ServerConfig.conf"),
				std::filesystem::path(L"ServerConfig.conf")
			};

			for (const auto& candidate : candidates)
			{
				if (std::filesystem::exists(candidate))
				{
					return std::filesystem::absolute(candidate);
				}
			}

			return {};
		}

		bool LoadServerConfig()
		{
			const std::filesystem::path configPath = FindConfigPath();
			if (configPath.empty())
			{
				std::wcerr << L"ServerConfig.conf 파일을 찾지 못했습니다.\n";
				return false;
			}

			MHLib::utils::CFileLoader loader;
			loader.Parse(configPath.c_str());
			loader.Load(L"Server", L"IP", &Config::serverIp);
			loader.Load(L"Server", L"PORT", &Config::serverPort);
			loader.Load(L"Server", L"PACKET_KEY", &MHLib::scurity::CEncryption::PACKET_KEY);
			loader.Load(L"Server", L"PACKET_CODE", &Config::packetCode);

			// 서버 설정 파일의 0.0.0.0은 바인드 주소이므로, 클라이언트 접속 대상은 로컬호스트로 보정한다.
			if (Config::serverIp.empty() || Config::serverIp == "0.0.0.0")
			{
				Config::serverIp = "127.0.0.1";
			}

			return true;
		}

		void ApplyArguments(int argc, char* argv[])
		{
			UNREFERENCED_PARAMETER(argc);
			UNREFERENCED_PARAMETER(argv);

			int wideArgCount = 0;
			LPWSTR* wideArgs = CommandLineToArgvW(GetCommandLineW(), &wideArgCount);
			if (wideArgs == nullptr)
			{
				return;
			}

			for (int i = 1; i < wideArgCount; ++i)
			{
				char argBuffer[128]{};
				WideCharToMultiByte(CP_UTF8, 0, wideArgs[i], -1, argBuffer, static_cast<int>(sizeof(argBuffer)), nullptr, nullptr);
				const std::string arg = argBuffer;

				if (arg == "--clients" && i + 1 < wideArgCount)
				{
					char valueBuffer[64]{};
					WideCharToMultiByte(CP_UTF8, 0, wideArgs[++i], -1, valueBuffer, static_cast<int>(sizeof(valueBuffer)), nullptr, nullptr);
					Config::parallelClientCount = max(1, atoi(valueBuffer));
				}
				else if (arg == "--repeat" && i + 1 < wideArgCount)
				{
					char valueBuffer[64]{};
					WideCharToMultiByte(CP_UTF8, 0, wideArgs[++i], -1, valueBuffer, static_cast<int>(sizeof(valueBuffer)), nullptr, nullptr);
					Config::repeatCount = max(1, atoi(valueBuffer));
				}
				else if (arg == "--ip" && i + 1 < wideArgCount)
				{
					char valueBuffer[128]{};
					WideCharToMultiByte(CP_UTF8, 0, wideArgs[++i], -1, valueBuffer, static_cast<int>(sizeof(valueBuffer)), nullptr, nullptr);
					Config::serverIp = valueBuffer;
				}
				else if (arg == "--port" && i + 1 < wideArgCount)
				{
					char valueBuffer[64]{};
					WideCharToMultiByte(CP_UTF8, 0, wideArgs[++i], -1, valueBuffer, static_cast<int>(sizeof(valueBuffer)), nullptr, nullptr);
					Config::serverPort = static_cast<USHORT>(atoi(valueBuffer));
				}
				else if (arg == "--hold-seconds" && i + 1 < wideArgCount)
				{
					char valueBuffer[64]{};
					WideCharToMultiByte(CP_UTF8, 0, wideArgs[++i], -1, valueBuffer, static_cast<int>(sizeof(valueBuffer)), nullptr, nullptr);
					Config::holdSeconds = max(0, atoi(valueBuffer));
				}
				else if (arg == "--heartbeat-ms" && i + 1 < wideArgCount)
				{
					char valueBuffer[64]{};
					WideCharToMultiByte(CP_UTF8, 0, wideArgs[++i], -1, valueBuffer, static_cast<int>(sizeof(valueBuffer)), nullptr, nullptr);
					Config::heartbeatIntervalMs = max(1000, atoi(valueBuffer));
				}
				else if (arg == "--no-wait")
				{
					Config::waitOnExit = false;
				}
			}

			LocalFree(wideArgs);
		}

		std::vector<char> BuildClientPacket(const std::vector<char>& payload)
		{
			std::vector<char> packet(sizeof(NetworkLib::Core::Utils::Net::Header) + payload.size());
			auto* header = reinterpret_cast<NetworkLib::Core::Utils::Net::Header*>(packet.data());

			header->code = Config::packetCode;
			header->len = static_cast<USHORT>(payload.size());
			header->randKey = 0;
			header->checkSum = MHLib::scurity::CEncryption::CalCheckSum(const_cast<char*>(payload.data()), static_cast<int>(payload.size()));

			if (!payload.empty())
			{
				memcpy(packet.data() + sizeof(NetworkLib::Core::Utils::Net::Header), payload.data(), payload.size());
			}

			MHLib::scurity::CEncryption::Encoding(packet.data() + 4, static_cast<int>(payload.size()) + 1, header->randKey);
			return packet;
		}

		std::vector<char> BuildLoginPayload(INT64 accountNo)
		{
			std::vector<char> payload;
			const WORD type = static_cast<WORD>(EchoServer::Protocol::PACKET_TYPE::CS_GAME_REQ_LOGIN);
			constexpr size_t sessionKeySize = 64;
			char sessionKeyBuffer[sessionKeySize]{};

			memcpy(sessionKeyBuffer, Config::sessionKey.data(), min(Config::sessionKey.size(), sessionKeySize));

			AppendValue(payload, type);
			AppendValue(payload, accountNo);
			AppendBytes(payload, sessionKeyBuffer, sizeof(sessionKeyBuffer));
			AppendValue(payload, Config::loginVersion);
			return payload;
		}

		std::vector<char> BuildEchoPayload(INT64 accountNo, LONGLONG sendTick)
		{
			std::vector<char> payload;
			const WORD type = static_cast<WORD>(EchoServer::Protocol::PACKET_TYPE::CS_GAME_REQ_ECHO);

			AppendValue(payload, type);
			AppendValue(payload, accountNo);
			AppendValue(payload, sendTick);
			return payload;
		}

		std::vector<char> BuildHeartbeatPayload()
		{
			std::vector<char> payload;
			const WORD type = static_cast<WORD>(EchoServer::Protocol::PACKET_TYPE::CS_GAME_REQ_HEARTBEAT);
			AppendValue(payload, type);
			return payload;
		}

		bool ReceivePacket(SOCKET socket, NetPacket& outPacket)
		{
			if (!RecvAll(socket, reinterpret_cast<char*>(&outPacket.header), sizeof(outPacket.header)))
			{
				return false;
			}

			if (outPacket.header.code != Config::packetCode)
			{
				return false;
			}

			std::vector<char> encoded(sizeof(BYTE) + outPacket.header.len);
			encoded[0] = outPacket.header.checkSum;
			if (outPacket.header.len > 0)
			{
				if (!RecvAll(socket, encoded.data() + 1, outPacket.header.len))
				{
					return false;
				}
			}

			MHLib::scurity::CEncryption::Decoding(encoded.data(), static_cast<int>(encoded.size()), outPacket.header.randKey);
			const BYTE decodedChecksum = static_cast<BYTE>(encoded[0]);

			outPacket.payload.assign(encoded.begin() + 1, encoded.end());
			const BYTE calculatedChecksum = MHLib::scurity::CEncryption::CalCheckSum(outPacket.payload.data(), static_cast<int>(outPacket.payload.size()));
			if (decodedChecksum != calculatedChecksum)
			{
				return false;
			}

			outPacket.header.checkSum = decodedChecksum;
			return true;
		}

		bool ParseLoginResponse(const NetPacket& packet, BYTE& outStatus, INT64& outAccountNo)
		{
			const size_t expectedSize = sizeof(WORD) + sizeof(BYTE) + sizeof(INT64);
			if (packet.payload.size() != expectedSize)
			{
				return false;
			}

			size_t offset = 0;
			WORD type = 0;
			memcpy(&type, packet.payload.data() + offset, sizeof(type));
			offset += sizeof(type);

			if (type != static_cast<WORD>(EchoServer::Protocol::PACKET_TYPE::CS_GAME_RES_LOGIN))
			{
				return false;
			}

			memcpy(&outStatus, packet.payload.data() + offset, sizeof(outStatus));
			offset += sizeof(outStatus);
			memcpy(&outAccountNo, packet.payload.data() + offset, sizeof(outAccountNo));
			return true;
		}

		bool ParseEchoResponse(const NetPacket& packet, INT64& outAccountNo, LONGLONG& outSendTick)
		{
			const size_t expectedSize = sizeof(WORD) + sizeof(INT64) + sizeof(LONGLONG);
			if (packet.payload.size() != expectedSize)
			{
				return false;
			}

			size_t offset = 0;
			WORD type = 0;
			memcpy(&type, packet.payload.data() + offset, sizeof(type));
			offset += sizeof(type);

			if (type != static_cast<WORD>(EchoServer::Protocol::PACKET_TYPE::CS_GAME_RES_ECHO))
			{
				return false;
			}

			memcpy(&outAccountNo, packet.payload.data() + offset, sizeof(outAccountNo));
			offset += sizeof(outAccountNo);
			memcpy(&outSendTick, packet.payload.data() + offset, sizeof(outSendTick));
			return true;
		}

		bool RunClientSequence(INT clientIndex)
		{
			SOCKET clientSocket = INVALID_SOCKET;
			INT64 accountNo = Config::accountNoBase + clientIndex;

			clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (clientSocket == INVALID_SOCKET)
			{
				std::cerr << "[Client " << clientIndex << "] socket 생성 실패\n";
				return false;
			}

			DWORD timeoutMs = 3000;
			setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));
			setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));

			SOCKADDR_IN serverAddress{};
			serverAddress.sin_family = AF_INET;
			serverAddress.sin_port = htons(Config::serverPort);
			InetPtonA(AF_INET, Config::serverIp.c_str(), &serverAddress.sin_addr);

			if (connect(clientSocket, reinterpret_cast<SOCKADDR*>(&serverAddress), sizeof(serverAddress)) == SOCKET_ERROR)
			{
				std::cerr << "[Client " << clientIndex << "] connect 실패: " << WSAGetLastError() << "\n";
				closesocket(clientSocket);
				return false;
			}

			bool isSuccess = true;
			for (INT repeat = 0; repeat < Config::repeatCount; ++repeat)
			{
				const auto loginPacket = BuildClientPacket(BuildLoginPayload(accountNo));
				if (!SendAll(clientSocket, loginPacket.data(), static_cast<int>(loginPacket.size())))
				{
					std::cerr << "[Client " << clientIndex << "] 로그인 패킷 전송 실패\n";
					isSuccess = false;
					break;
				}

				NetPacket loginResponse;
				if (!ReceivePacket(clientSocket, loginResponse))
				{
					std::cerr << "[Client " << clientIndex << "] 로그인 응답 수신 실패\n";
					isSuccess = false;
					break;
				}

				BYTE loginStatus = FALSE;
				INT64 loginAccountNo = 0;
				if (!ParseLoginResponse(loginResponse, loginStatus, loginAccountNo) || loginStatus == FALSE || loginAccountNo != accountNo)
				{
					std::cerr << "[Client " << clientIndex << "] 로그인 응답 검증 실패\n";
					isSuccess = false;
					break;
				}

				const LONGLONG sendTick = GetTickCount64();
				const auto echoPacket = BuildClientPacket(BuildEchoPayload(accountNo, sendTick));
				if (!SendAll(clientSocket, echoPacket.data(), static_cast<int>(echoPacket.size())))
				{
					std::cerr << "[Client " << clientIndex << "] 에코 패킷 전송 실패\n";
					isSuccess = false;
					break;
				}

				NetPacket echoResponse;
				if (!ReceivePacket(clientSocket, echoResponse))
				{
					std::cerr << "[Client " << clientIndex << "] 에코 응답 수신 실패\n";
					isSuccess = false;
					break;
				}

				INT64 echoAccountNo = 0;
				LONGLONG echoSendTick = 0;
				if (!ParseEchoResponse(echoResponse, echoAccountNo, echoSendTick) || echoAccountNo != accountNo || echoSendTick != sendTick)
				{
					std::cerr << "[Client " << clientIndex << "] 에코 응답 검증 실패\n";
					isSuccess = false;
					break;
				}
			}

			shutdown(clientSocket, SD_BOTH);
			closesocket(clientSocket);

			if (isSuccess)
			{
				std::cout << "[Client " << clientIndex << "] login/echo 검증 성공\n";
			}

			return isSuccess;
		}

		bool RunInteractiveSequence(INT clientIndex)
		{
			SOCKET clientSocket = INVALID_SOCKET;
			INT64 accountNo = Config::accountNoBase + clientIndex;

			clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (clientSocket == INVALID_SOCKET)
			{
				std::cerr << "[Client " << clientIndex << "] socket 생성 실패\n";
				return false;
			}

			DWORD timeoutMs = 3000;
			setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));
			setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));

			SOCKADDR_IN serverAddress{};
			serverAddress.sin_family = AF_INET;
			serverAddress.sin_port = htons(Config::serverPort);
			InetPtonA(AF_INET, Config::serverIp.c_str(), &serverAddress.sin_addr);

			if (connect(clientSocket, reinterpret_cast<SOCKADDR*>(&serverAddress), sizeof(serverAddress)) == SOCKET_ERROR)
			{
				std::cerr << "[Client " << clientIndex << "] connect 실패: " << WSAGetLastError() << "\n";
				closesocket(clientSocket);
				return false;
			}

			std::cout << "[Client " << clientIndex << "] 서버 접속 성공\n";

			bool isSuccess = true;
			const auto loginPacket = BuildClientPacket(BuildLoginPayload(accountNo));
			if (!SendAll(clientSocket, loginPacket.data(), static_cast<int>(loginPacket.size())))
			{
				std::cerr << "[Client " << clientIndex << "] 로그인 패킷 전송 실패\n";
				isSuccess = false;
			}
			else
			{
				NetPacket loginResponse;
				if (!ReceivePacket(clientSocket, loginResponse))
				{
					std::cerr << "[Client " << clientIndex << "] 로그인 응답 수신 실패\n";
					isSuccess = false;
				}
				else
				{
					BYTE loginStatus = FALSE;
					INT64 loginAccountNo = 0;
					if (!ParseLoginResponse(loginResponse, loginStatus, loginAccountNo) || loginStatus == FALSE || loginAccountNo != accountNo)
					{
						std::cerr << "[Client " << clientIndex << "] 로그인 응답 검증 실패\n";
						isSuccess = false;
					}
					else
					{
						std::cout << "[Client " << clientIndex << "] 로그인 성공, accountNo=" << loginAccountNo << "\n";
					}
				}
			}

			for (INT repeat = 0; isSuccess && repeat < Config::repeatCount; ++repeat)
			{
				const LONGLONG sendTick = GetTickCount64();
				const auto echoPacket = BuildClientPacket(BuildEchoPayload(accountNo, sendTick));
				if (!SendAll(clientSocket, echoPacket.data(), static_cast<int>(echoPacket.size())))
				{
					std::cerr << "[Client " << clientIndex << "] 에코 패킷 전송 실패\n";
					isSuccess = false;
					break;
				}

				NetPacket echoResponse;
				if (!ReceivePacket(clientSocket, echoResponse))
				{
					std::cerr << "[Client " << clientIndex << "] 에코 응답 수신 실패\n";
					isSuccess = false;
					break;
				}

				INT64 echoAccountNo = 0;
				LONGLONG echoSendTick = 0;
				if (!ParseEchoResponse(echoResponse, echoAccountNo, echoSendTick) || echoAccountNo != accountNo || echoSendTick != sendTick)
				{
					std::cerr << "[Client " << clientIndex << "] 에코 응답 검증 실패\n";
					isSuccess = false;
					break;
				}

				std::cout << "[Client " << clientIndex << "] echo " << (repeat + 1) << "/" << Config::repeatCount << " 성공\n";
			}

			if (isSuccess && Config::holdSeconds > 0)
			{
				const auto heartbeatPacket = BuildClientPacket(BuildHeartbeatPayload());
				const auto holdStart = std::chrono::steady_clock::now();
				auto nextHeartbeat = holdStart;

				std::cout << "[Client " << clientIndex << "] " << Config::holdSeconds << "초 동안 연결 유지 시작\n";

				while (std::chrono::steady_clock::now() - holdStart < std::chrono::seconds(Config::holdSeconds))
				{
					const auto now = std::chrono::steady_clock::now();
					if (now >= nextHeartbeat)
					{
						if (!SendAll(clientSocket, heartbeatPacket.data(), static_cast<int>(heartbeatPacket.size())))
						{
							std::cerr << "[Client " << clientIndex << "] 하트비트 전송 실패\n";
							isSuccess = false;
							break;
						}

						std::cout << "[Client " << clientIndex << "] heartbeat 전송\n";
						nextHeartbeat = now + std::chrono::milliseconds(Config::heartbeatIntervalMs);
					}

					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}
			}

			shutdown(clientSocket, SD_BOTH);
			closesocket(clientSocket);

			if (isSuccess)
			{
				std::cout << "[Client " << clientIndex << "] 테스트 시퀀스 성공\n";
			}

			return isSuccess;
		}
	}
}

int main(int argc, char* argv[])
{
	WSADATA wsaData{};
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		std::cerr << "WSAStartup 실패\n";
		return 1;
	}

	if (!TestClient::Detail::LoadServerConfig())
	{
		WSACleanup();
		return 1;
	}

	TestClient::Detail::ApplyArguments(argc, argv);

	std::cout << "Server IP: " << TestClient::Config::serverIp
		<< ", Port: " << TestClient::Config::serverPort
		<< ", Clients: " << TestClient::Config::parallelClientCount
		<< ", Repeat: " << TestClient::Config::repeatCount
		<< ", HoldSeconds: " << TestClient::Config::holdSeconds
		<< ", HeartbeatMs: " << TestClient::Config::heartbeatIntervalMs << "\n";

	std::vector<std::thread> workers;
	std::vector<int> results(static_cast<size_t>(TestClient::Config::parallelClientCount), FALSE);
	for (INT clientIndex = 0; clientIndex < TestClient::Config::parallelClientCount; ++clientIndex)
	{
		workers.emplace_back([clientIndex, &results]()
		{
			results[static_cast<size_t>(clientIndex)] = TestClient::Detail::RunInteractiveSequence(clientIndex) ? TRUE : FALSE;
		});
	}

	for (auto& worker : workers)
	{
		worker.join();
	}

	WSACleanup();

	const bool hasFailure = std::find(results.begin(), results.end(), FALSE) != results.end();
	std::cout << (hasFailure ? "테스트 실패가 있습니다.\n" : "모든 테스트가 성공했습니다.\n");

	if (TestClient::Config::waitOnExit)
	{
		std::cout << "종료하려면 Enter 키를 누르세요...";
		std::cin.get();
	}

	return hasFailure ? 1 : 0;
}
