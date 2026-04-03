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
		FContentThread(Core::IContent& content, Bridge::IContentBridge& bridge, const Core::SContentRuntimeConfig& config);
		~FContentThread();

		void Start();
		void Stop();
		Core::SContentThreadStats GetStatsSnapshot();

		void EnqueueEnter(Core::SContentLifecycleEvent event);
		void EnqueueLeave(Core::SContentLifecycleEvent event);
		void EnqueuePacket(Core::FOwnedPacketEnvelope&& packet);

	private:
		struct SImpl;
		std::unique_ptr<SImpl> m_impl;
	};
}
