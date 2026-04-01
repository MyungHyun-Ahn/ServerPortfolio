#include "Pch.h"

#include "Containers/FLockFreeQueue.h"
#include "Containers/FLockFreeStack.h"
#include "Memory/FTlsMemoryPool.h"

namespace
{
	struct FLockFreeSmokeNode
	{
		int value = 0;
	};

	void RunLockFreeSmoke() noexcept
	{
		GameServer::NetworkLib::Containers::FLockFreeStack<int> stack;
		stack.Push(10);
		int stackValue = 0;
		stack.Pop(stackValue);

		GameServer::NetworkLib::Containers::FLockFreeQueue<int> queue;
		queue.Enqueue(20);
		int queueValue = 0;
		queue.Dequeue(queueValue);

		GameServer::NetworkLib::Memory::FTlsMemoryPoolManager<FLockFreeSmokeNode> pool;
		FLockFreeSmokeNode* node = pool.Alloc();
		node->value = stackValue + queueValue;
		pool.Free(node);
	}

	const int kRunSmokeResult = []() noexcept
	{
		RunLockFreeSmoke();
		return 0;
	}();
}