#include "NetworkLib/Containers/FLockFreeQueue.h"
#include "NetworkLib/Containers/FLockFreeStack.h"
#include "NetworkLib/Memory/FTlsMemoryPool.h"

#include <atomic>
#include <cstdint>
#include <iostream>
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

	bool allPassed = true;
	for (const STestResult& result : results)
	{
		std::cout << "[" << (result.passed ? "PASS" : "FAIL") << "] " << result.name << " : " << result.detail << "\n";
		allPassed = allPassed && result.passed;
	}

	return allPassed ? 0 : 1;
}
