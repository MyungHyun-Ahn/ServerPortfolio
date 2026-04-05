#pragma once

#include "ContentsRuntime/Core/ContentRuntimeTypes.h"

namespace ContentsRuntime::Bridge
{
	class IContentBridge;
}

namespace ContentsRuntime::Core
{
	class IContent;
}

namespace ContentsRuntime::Threading
{
	class FContentThread
	{
	public:
		FContentThread(Bridge::IContentBridge& bridge, const Core::SContentRuntimeConfig& config, std::uint32_t workerIndex);
		~FContentThread();

		bool RegisterContent(Core::IContent& content);
		void Start();
		void Stop();
		Core::SContentThreadStats GetStatsSnapshot(Core::FContentInstanceId contentInstanceId);
		std::uint32_t GetWorkerIndex() const noexcept;

		void EnqueueEnter(Core::SContentLifecycleEvent event);
		void EnqueueLeave(Core::SContentLifecycleEvent event);
		void EnqueuePacket(Core::FOwnedPacketEnvelope&& packet);

	private:
		struct SImpl;
		std::unique_ptr<SImpl> m_impl;
	};
}
