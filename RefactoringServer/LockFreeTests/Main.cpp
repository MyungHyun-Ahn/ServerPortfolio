#include "Containers/FLockFreeQueue.h"
#include "Containers/FLockFreeStack.h"
#include "Crypto/FDefaultPacketCipher.h"
#include "Crypto/FNullPacketCipher.h"
#include "Generated/Packets/Echo/EchoPackets.h"
#include "Crypto/IPacketCipher.h"
#include "Memory/FTlsMemoryPool.h"
#include "Packet/FDefaultPacketFramer.h"
#include "Packet/FPacketSerialization.h"
#include "Packet/FPacketView.h"
#include "Packet/FRecvBuffer.h"

#include <atomic>
#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

namespace
{
	struct STestResult
	{
		bool passed = false;
		std::string name;
		std::string detail;
	};

	using TQueue = GameServer::NetworkLib::Containers::FLockFreeQueue<int>;
	using TStack = GameServer::NetworkLib::Containers::FLockFreeStack<int>;
	static_assert(sizeof(TQueue) == sizeof(std::int64_t) * 2, "FLockFreeQueue instance should only keep head and tail pointers.");
	static_assert(sizeof(TStack) == sizeof(std::int64_t), "FLockFreeStack instance should only keep top pointer.");

	struct STlsPoolPayload
	{
		std::int32_t ownerThread = -1;
		std::int32_t iteration = -1;
		char padding[48]{};
	};

	template <typename TContainer>
	STestResult RunLinearQueueTest()
	{
		TContainer queue;
		for (int value = 1; value <= 1000; ++value)
		{
			queue.Enqueue(value);
		}

		for (int expected = 1; expected <= 1000; ++expected)
		{
			int dequeuedValue = 0;
			if (!queue.Dequeue(dequeuedValue))
			{
				return { false, "Queue linear FIFO", "dequeue failed before queue was empty" };
			}

			if (dequeuedValue != expected)
			{
				return { false, "Queue linear FIFO", "FIFO order mismatch" };
			}
		}

		int tailValue = 0;
		if (queue.Dequeue(tailValue))
		{
			return { false, "Queue linear FIFO", "queue should be empty after 1000 dequeues" };
		}

		return { true, "Queue linear FIFO", "passed" };
	}

	template <typename TContainer>
	STestResult RunParallelSumTest(const char* testName)
	{
		constexpr int kProducerCount = 4;
		constexpr int kConsumerCount = 4;
		constexpr int kItemsPerProducer = 50000;
		const std::int64_t expectedCount = static_cast<std::int64_t>(kProducerCount) * kItemsPerProducer;

		TContainer container;
		std::atomic<std::int64_t> producedSum = 0;
		std::atomic<std::int64_t> consumedSum = 0;
		std::atomic<std::int64_t> consumedCount = 0;
		std::atomic<int> activeProducers = kProducerCount;

		std::vector<std::thread> producerThreads;
		producerThreads.reserve(kProducerCount);
		for (int producerIndex = 0; producerIndex < kProducerCount; ++producerIndex)
		{
			producerThreads.emplace_back([&, producerIndex]()
			{
				const int baseValue = producerIndex * kItemsPerProducer;
				for (int offset = 1; offset <= kItemsPerProducer; ++offset)
				{
					const int value = baseValue + offset;
					container.Push(value);
					producedSum.fetch_add(value, std::memory_order_relaxed);
				}

				activeProducers.fetch_sub(1, std::memory_order_release);
			});
		}

		std::vector<std::thread> consumerThreads;
		consumerThreads.reserve(kConsumerCount);
		for (int consumerIndex = 0; consumerIndex < kConsumerCount; ++consumerIndex)
		{
			consumerThreads.emplace_back([&]()
			{
				while (true)
				{
					int value = 0;
					if (container.Pop(value))
					{
						consumedSum.fetch_add(value, std::memory_order_relaxed);
						const std::int64_t newCount = consumedCount.fetch_add(1, std::memory_order_relaxed) + 1;
						if (newCount >= expectedCount)
						{
							return;
						}

						continue;
					}

					if (activeProducers.load(std::memory_order_acquire) == 0 &&
						consumedCount.load(std::memory_order_relaxed) >= expectedCount)
					{
						return;
					}

					std::this_thread::yield();
				}
			});
		}

		for (auto& producerThread : producerThreads)
		{
			producerThread.join();
		}

		for (auto& consumerThread : consumerThreads)
		{
			consumerThread.join();
		}

		if (consumedCount.load() != expectedCount)
		{
			return { false, testName, "consumed item count mismatch" };
		}

		if (consumedSum.load() != producedSum.load())
		{
			return { false, testName, "sum mismatch after parallel run" };
		}

		return { true, testName, "passed" };
	}

	STestResult RunTlsMemoryPoolParallelTest()
	{
		constexpr int kThreadCount = 8;
		constexpr int kBatchSize = 64;
		constexpr int kIterationCount = 10000;

		GameServer::NetworkLib::Memory::FTlsMemoryPoolManager<STlsPoolPayload, kBatchSize, 2> memoryPool;
		std::atomic<bool> encounteredError = false;
		std::vector<std::thread> workerThreads;
		workerThreads.reserve(kThreadCount);

		for (int threadIndex = 0; threadIndex < kThreadCount; ++threadIndex)
		{
			workerThreads.emplace_back([&, threadIndex]()
			{
				std::vector<STlsPoolPayload*> batch;
				batch.reserve(kBatchSize);

				for (int iteration = 0; iteration < kIterationCount; ++iteration)
				{
					for (int batchIndex = 0; batchIndex < kBatchSize; ++batchIndex)
					{
						STlsPoolPayload* payload = memoryPool.Alloc();
						if (payload == nullptr)
						{
							encounteredError.store(true, std::memory_order_relaxed);
							return;
						}

						payload->ownerThread = threadIndex;
						payload->iteration = iteration;
						batch.push_back(payload);
					}

					for (STlsPoolPayload* payload : batch)
					{
						if (payload->ownerThread != threadIndex || payload->iteration != iteration)
						{
							encounteredError.store(true, std::memory_order_relaxed);
							return;
						}

						memoryPool.Free(payload);
					}

					batch.clear();
				}
			});
		}

		for (auto& workerThread : workerThreads)
		{
			workerThread.join();
		}

		if (encounteredError.load(std::memory_order_relaxed))
		{
			return { false, "TLS memory pool parallel", "allocation or payload validation failed" };
		}

		if (memoryPool.GetUseCount() != 0)
		{
			return { false, "TLS memory pool parallel", "pool usage should return to zero after frees" };
		}

		STlsPoolPayload* finalPayload = memoryPool.Alloc();
		if (finalPayload == nullptr)
		{
			return { false, "TLS memory pool parallel", "post-run allocation failed" };
		}

		memoryPool.Free(finalPayload);
		return { true, "TLS memory pool parallel", "passed" };
	}

	STestResult RunPacketCipherRoundTripTest()
	{
		using namespace GameServer::NetworkLib::Crypto;

		SDefaultPacketCipherConfig config{};
		config.enabled = true;
		config.packetKey = 0x37;

		std::shared_ptr<IPacketCipher> cipher = std::make_shared<FDefaultPacketCipher>(config);
		std::string originalMessage = "packet-cipher-roundtrip";
		std::string encodedMessage = originalMessage;
		const std::uint8_t randomKey = 0x5A;

		if (!cipher->GetConfig().enabled)
		{
			return { false, "Packet cipher round trip", "cipher config should be visible through interface" };
		}

		const std::uint8_t originalChecksum = cipher->CalculateChecksum(originalMessage.data(), static_cast<int>(originalMessage.size()));
		cipher->Encode(encodedMessage.data(), static_cast<int>(encodedMessage.size()), randomKey);
		if (encodedMessage == originalMessage)
		{
			return { false, "Packet cipher round trip", "encoded message should differ from original plaintext" };
		}

		cipher->Decode(encodedMessage.data(), static_cast<int>(encodedMessage.size()), randomKey);
		if (encodedMessage != originalMessage)
		{
			return { false, "Packet cipher round trip", "decoded message does not match original plaintext" };
		}

		const std::uint8_t decodedChecksum = cipher->CalculateChecksum(encodedMessage.data(), static_cast<int>(encodedMessage.size()));
		if (decodedChecksum != originalChecksum)
		{
			return { false, "Packet cipher round trip", "checksum mismatch after decode" };
		}

		return { true, "Packet cipher round trip", "passed" };
	}

	STestResult RunNullPacketCipherTest()
	{
		using namespace GameServer::NetworkLib::Crypto;

		SPacketCipherConfig config{};
		config.enabled = false;

		std::shared_ptr<IPacketCipher> cipher = std::make_shared<FNullPacketCipher>(config);
		std::string originalMessage = "packet-cipher-null";
		std::string transformedMessage = originalMessage;

		const std::uint8_t originalChecksum = cipher->CalculateChecksum(originalMessage.data(), static_cast<int>(originalMessage.size()));
		cipher->Encode(transformedMessage.data(), static_cast<int>(transformedMessage.size()), 0x11);
		if (transformedMessage != originalMessage)
		{
			return { false, "Null packet cipher", "encode should not modify payload" };
		}

		cipher->Decode(transformedMessage.data(), static_cast<int>(transformedMessage.size()), 0x22);
		if (transformedMessage != originalMessage)
		{
			return { false, "Null packet cipher", "decode should not modify payload" };
		}

		const std::uint8_t transformedChecksum = cipher->CalculateChecksum(transformedMessage.data(), static_cast<int>(transformedMessage.size()));
		if (transformedChecksum != originalChecksum)
		{
			return { false, "Null packet cipher", "checksum should remain stable for no-op cipher" };
		}

		return { true, "Null packet cipher", "passed" };
	}

	STestResult RunPacketFramerRoundTripTest()
	{
		using namespace GameServer::NetworkLib::Packet;

		FDefaultPacketFramer framer;
		std::vector<char> packetBuffer;
		const std::string payload = "framed-echo-payload";
		std::vector<char> contentPayload = BuildContentPayload(3001, std::vector<char>(payload.begin(), payload.end()));
		SOutgoingPacket outgoingPacket{};
		outgoingPacket.randomKey = 0x44;
		outgoingPacket.checkSum = CalculatePacketChecksum(contentPayload.data(), static_cast<std::int32_t>(contentPayload.size()));
		outgoingPacket.payload = contentPayload.data();
		outgoingPacket.payloadLength = static_cast<std::int32_t>(contentPayload.size());

		if (!framer.BuildPacket(outgoingPacket, packetBuffer))
		{
			return { false, "Packet framer round trip", "BuildPacket failed" };
		}

		if (packetBuffer.size() != contentPayload.size() + sizeof(SPacketHeader))
		{
			return { false, "Packet framer round trip", "packet size mismatch" };
		}

		SFramedPacket framedPacket;
		std::vector<char> receiveBuffer = packetBuffer;
		if (!framer.TryExtractPacket(receiveBuffer, framedPacket))
		{
			return { false, "Packet framer round trip", "TryExtractPacket failed" };
		}

		if (!receiveBuffer.empty())
		{
			return { false, "Packet framer round trip", "receive buffer should be empty after extraction" };
		}

		if (framedPacket.randomKey != 0x44)
		{
			return { false, "Packet framer round trip", "randomKey mismatch after extraction" };
		}

		if (framedPacket.checkSum != outgoingPacket.checkSum)
		{
			return { false, "Packet framer round trip", "checksum mismatch after extraction" };
		}

		FPacketView transportPacketView{};
		transportPacketView.randomKey = framedPacket.randomKey;
		transportPacketView.checkSum = framedPacket.checkSum;
		transportPacketView.payload = framedPacket.payload.data();
		transportPacketView.payloadLength = static_cast<std::int32_t>(framedPacket.payload.size());

		FPacketView contentPacketView{};
		if (!TryParseContentPacketView(transportPacketView, contentPacketView))
		{
			return { false, "Packet framer round trip", "content packet view parse failed" };
		}

		if (contentPacketView.opcode != 3001)
		{
			return { false, "Packet framer round trip", "opcode mismatch after extraction" };
		}

		if (std::string(contentPacketView.payload, contentPacketView.payload + contentPacketView.payloadLength) != payload)
		{
			return { false, "Packet framer round trip", "payload mismatch after extraction" };
		}

		return { true, "Packet framer round trip", "passed" };
	}

	STestResult RunPacketFramerPartialReceiveTest()
	{
		using namespace GameServer::NetworkLib::Packet;

		FDefaultPacketFramer framer;
		std::vector<char> packetBuffer;
		const std::string payload = "partial-frame";
		std::vector<char> contentPayload = BuildContentPayload(3002, std::vector<char>(payload.begin(), payload.end()));
		SOutgoingPacket outgoingPacket{};
		outgoingPacket.randomKey = 0x21;
		outgoingPacket.checkSum = CalculatePacketChecksum(contentPayload.data(), static_cast<std::int32_t>(contentPayload.size()));
		outgoingPacket.payload = contentPayload.data();
		outgoingPacket.payloadLength = static_cast<std::int32_t>(contentPayload.size());

		if (!framer.BuildPacket(outgoingPacket, packetBuffer))
		{
			return { false, "Packet framer partial receive", "BuildPacket failed" };
		}

		std::vector<char> receiveBuffer(packetBuffer.begin(), packetBuffer.begin() + 2);
		SFramedPacket framedPacket;
		if (framer.TryExtractPacket(receiveBuffer, framedPacket))
		{
			return { false, "Packet framer partial receive", "packet should not be extracted before header is complete" };
		}

		receiveBuffer.insert(receiveBuffer.end(), packetBuffer.begin() + 2, packetBuffer.end() - 3);
		if (framer.TryExtractPacket(receiveBuffer, framedPacket))
		{
			return { false, "Packet framer partial receive", "packet should not be extracted before payload is complete" };
		}

		receiveBuffer.insert(receiveBuffer.end(), packetBuffer.end() - 3, packetBuffer.end());
		if (!framer.TryExtractPacket(receiveBuffer, framedPacket))
		{
			return { false, "Packet framer partial receive", "packet should be extracted after remaining bytes arrive" };
		}

		if (framedPacket.checkSum != outgoingPacket.checkSum)
		{
			return { false, "Packet framer partial receive", "checksum mismatch after partial receive assembly" };
		}

		FPacketView transportPacketView{};
		transportPacketView.randomKey = framedPacket.randomKey;
		transportPacketView.checkSum = framedPacket.checkSum;
		transportPacketView.payload = framedPacket.payload.data();
		transportPacketView.payloadLength = static_cast<std::int32_t>(framedPacket.payload.size());

		FPacketView contentPacketView{};
		if (!TryParseContentPacketView(transportPacketView, contentPacketView))
		{
			return { false, "Packet framer partial receive", "content packet view parse failed" };
		}

		if (contentPacketView.opcode != 3002)
		{
			return { false, "Packet framer partial receive", "opcode mismatch after partial receive assembly" };
		}

		if (std::string(contentPacketView.payload, contentPacketView.payload + contentPacketView.payloadLength) != payload)
		{
			return { false, "Packet framer partial receive", "payload mismatch after partial receive assembly" };
		}

		return { true, "Packet framer partial receive", "passed" };
	}

	STestResult RunPacketFramerRecvBufferTest()
	{
		using namespace GameServer::NetworkLib::Packet;

		FDefaultPacketFramer framer;
		FRecvBuffer recvBuffer(64);
		std::vector<char> packetBuffer;
		const std::string payload = "recv-ring-buffer";
		std::vector<char> contentPayload = BuildContentPayload(3003, std::vector<char>(payload.begin(), payload.end()));

		SOutgoingPacket outgoingPacket{};
		outgoingPacket.randomKey = 0x31;
		outgoingPacket.checkSum = CalculatePacketChecksum(contentPayload.data(), static_cast<std::int32_t>(contentPayload.size()));
		outgoingPacket.payload = contentPayload.data();
		outgoingPacket.payloadLength = static_cast<std::int32_t>(contentPayload.size());

		if (!framer.BuildPacket(outgoingPacket, packetBuffer))
		{
			return { false, "Packet framer recv buffer", "BuildPacket failed" };
		}

		WSABUF recvWsabufs[2]{};
		DWORD recvBufferCount = 0;
		recvBuffer.BuildRecvWsabufs(recvWsabufs, recvBufferCount);
		if (recvBufferCount != 1)
		{
			return { false, "Packet framer recv buffer", "unexpected initial recv buffer count" };
		}

		std::memcpy(recvWsabufs[0].buf, packetBuffer.data(), 5);
		if (!recvBuffer.CommitWrite(5))
		{
			return { false, "Packet framer recv buffer", "CommitWrite failed for first fragment" };
		}

		SFramedPacket framedPacket;
		if (framer.TryExtractPacket(recvBuffer, framedPacket))
		{
			return { false, "Packet framer recv buffer", "packet should not be extracted before full payload arrives" };
		}

		recvBuffer.BuildRecvWsabufs(recvWsabufs, recvBufferCount);
		std::memcpy(recvWsabufs[0].buf, packetBuffer.data() + 5, packetBuffer.size() - 5);
		if (!recvBuffer.CommitWrite(packetBuffer.size() - 5))
		{
			return { false, "Packet framer recv buffer", "CommitWrite failed for second fragment" };
		}

		if (!framer.TryExtractPacket(recvBuffer, framedPacket))
		{
			return { false, "Packet framer recv buffer", "packet should be extracted from recv buffer" };
		}

		FPacketView transportPacketView{};
		transportPacketView.randomKey = framedPacket.randomKey;
		transportPacketView.checkSum = framedPacket.checkSum;
		transportPacketView.payload = framedPacket.payload.data();
		transportPacketView.payloadLength = static_cast<std::int32_t>(framedPacket.payload.size());

		FPacketView contentPacketView{};
		if (!TryParseContentPacketView(transportPacketView, contentPacketView))
		{
			return { false, "Packet framer recv buffer", "content packet view parse failed" };
		}

		if (contentPacketView.opcode != 3003)
		{
			return { false, "Packet framer recv buffer", "opcode mismatch after recv buffer extraction" };
		}

		if (std::string(contentPacketView.payload, contentPacketView.payload + contentPacketView.payloadLength) != payload)
		{
			return { false, "Packet framer recv buffer", "payload mismatch after recv buffer extraction" };
		}

		if (recvBuffer.GetUsedSize() != 0)
		{
			return { false, "Packet framer recv buffer", "recv buffer should be empty after extraction" };
		}

		return { true, "Packet framer recv buffer", "passed" };
	}

	STestResult RunPacketFramerPacketViewTest()
	{
		using namespace GameServer::NetworkLib::Packet;

		FDefaultPacketFramer framer;
		FRecvBuffer recvBuffer(64);
		std::vector<char> packetBuffer;
		const std::string payload = "packet-view";
		std::vector<char> contentPayload = BuildContentPayload(3004, std::vector<char>(payload.begin(), payload.end()));

		SOutgoingPacket outgoingPacket{};
		outgoingPacket.randomKey = 0x55;
		outgoingPacket.checkSum = CalculatePacketChecksum(contentPayload.data(), static_cast<std::int32_t>(contentPayload.size()));
		outgoingPacket.payload = contentPayload.data();
		outgoingPacket.payloadLength = static_cast<std::int32_t>(contentPayload.size());

		if (!framer.BuildPacket(outgoingPacket, packetBuffer))
		{
			return { false, "Packet framer packet view", "BuildPacket failed" };
		}

		WSABUF recvWsabufs[2]{};
		DWORD recvBufferCount = 0;
		recvBuffer.BuildRecvWsabufs(recvWsabufs, recvBufferCount);
		std::memcpy(recvWsabufs[0].buf, packetBuffer.data(), packetBuffer.size());
		if (!recvBuffer.CommitWrite(packetBuffer.size()))
		{
			return { false, "Packet framer packet view", "CommitWrite failed" };
		}

		FPacketView packetView;
		if (!framer.TryExtractPacketView(recvBuffer, packetView))
		{
			return { false, "Packet framer packet view", "TryExtractPacketView failed" };
		}

		if (packetView.opcode != 0)
		{
			return { false, "Packet framer packet view", "transport packet view opcode should not be set" };
		}

		FPacketView contentPacketView{};
		if (!TryParseContentPacketView(packetView, contentPacketView))
		{
			return { false, "Packet framer packet view", "content packet view parse failed" };
		}

		const char* expectedPayloadPtr = recvBuffer.GetReadPointer() + sizeof(SPacketHeader) + sizeof(SContentHeader);
		if (contentPacketView.payload != expectedPayloadPtr)
		{
			return { false, "Packet framer packet view", "payload pointer should point into recv buffer" };
		}

		if (contentPacketView.opcode != 3004)
		{
			return { false, "Packet framer packet view", "opcode mismatch" };
		}

		if (std::string(contentPacketView.payload, contentPacketView.payload + contentPacketView.payloadLength) != payload)
		{
			return { false, "Packet framer packet view", "payload mismatch" };
		}

		if (!recvBuffer.Discard(sizeof(SPacketHeader) + static_cast<std::size_t>(packetView.payloadLength)))
		{
			return { false, "Packet framer packet view", "Discard failed after view dispatch" };
		}

		return { true, "Packet framer packet view", "passed" };
	}

	STestResult RunGeneratedEchoPacketRoundTripTest()
	{
		GameServer::Generated::Echo::FEchoRq requestPacket;
		requestPacket.message = "generated-echo-message";

		std::vector<char> payload = GameServer::NetworkLib::Packet::SerializeContentBody(requestPacket);
		GameServer::Generated::Echo::FEchoRq decodedPacket;
		if (!GameServer::NetworkLib::Packet::DeserializeContentPacket(payload.data(), payload.size(), decodedPacket))
		{
			return { false, "Generated echo packet round trip", "DeserializeContentPacket failed" };
		}

		if (decodedPacket.GetOpcode() != requestPacket.GetOpcode())
		{
			return { false, "Generated echo packet round trip", "opcode mismatch" };
		}

		if (decodedPacket.message != requestPacket.message)
		{
			return { false, "Generated echo packet round trip", "message mismatch" };
		}

		return { true, "Generated echo packet round trip", "passed" };
	}

	template <>
	STestResult RunParallelSumTest<TQueue>(const char* testName)
	{
		constexpr int kProducerCount = 4;
		constexpr int kConsumerCount = 4;
		constexpr int kItemsPerProducer = 50000;
		const std::int64_t expectedCount = static_cast<std::int64_t>(kProducerCount) * kItemsPerProducer;

		TQueue queue;
		std::atomic<std::int64_t> producedSum = 0;
		std::atomic<std::int64_t> consumedSum = 0;
		std::atomic<std::int64_t> consumedCount = 0;
		std::atomic<int> activeProducers = kProducerCount;

		std::vector<std::thread> producerThreads;
		for (int producerIndex = 0; producerIndex < kProducerCount; ++producerIndex)
		{
			producerThreads.emplace_back([&, producerIndex]()
			{
				const int baseValue = producerIndex * kItemsPerProducer;
				for (int offset = 1; offset <= kItemsPerProducer; ++offset)
				{
					const int value = baseValue + offset;
					queue.Enqueue(value);
					producedSum.fetch_add(value, std::memory_order_relaxed);
				}

				activeProducers.fetch_sub(1, std::memory_order_release);
			});
		}

		std::vector<std::thread> consumerThreads;
		for (int consumerIndex = 0; consumerIndex < kConsumerCount; ++consumerIndex)
		{
			consumerThreads.emplace_back([&]()
			{
				while (true)
				{
					int value = 0;
					if (queue.Dequeue(value))
					{
						consumedSum.fetch_add(value, std::memory_order_relaxed);
						const std::int64_t newCount = consumedCount.fetch_add(1, std::memory_order_relaxed) + 1;
						if (newCount >= expectedCount)
						{
							return;
						}

						continue;
					}

					if (activeProducers.load(std::memory_order_acquire) == 0 &&
						consumedCount.load(std::memory_order_relaxed) >= expectedCount)
					{
						return;
					}

					std::this_thread::yield();
				}
			});
		}

		for (auto& producerThread : producerThreads)
		{
			producerThread.join();
		}

		for (auto& consumerThread : consumerThreads)
		{
			consumerThread.join();
		}

		if (consumedCount.load() != expectedCount)
		{
			return { false, testName, "consumed item count mismatch" };
		}

		if (consumedSum.load() != producedSum.load())
		{
			return { false, testName, "sum mismatch after parallel run" };
		}

		return { true, testName, "passed" };
	}
}

int main()
{
	std::vector<STestResult> results;
	results.push_back(RunLinearQueueTest<TQueue>());
	results.push_back(RunParallelSumTest<TQueue>("Queue parallel sum"));
	results.push_back(RunParallelSumTest<TStack>("Stack parallel sum"));
	results.push_back(RunTlsMemoryPoolParallelTest());
	results.push_back(RunPacketCipherRoundTripTest());
	results.push_back(RunNullPacketCipherTest());
	results.push_back(RunPacketFramerRoundTripTest());
	results.push_back(RunPacketFramerPartialReceiveTest());
	results.push_back(RunPacketFramerRecvBufferTest());
	results.push_back(RunPacketFramerPacketViewTest());
	results.push_back(RunGeneratedEchoPacketRoundTripTest());

	bool allPassed = true;
	for (const STestResult& result : results)
	{
		std::cout << "[" << (result.passed ? "PASS" : "FAIL") << "] " << result.name << " : " << result.detail << "\n";
		allPassed = allPassed && result.passed;
	}

	return allPassed ? 0 : 1;
}
