#pragma once

#include "ContentsRuntime/Core/ContentTypes.h"

namespace ContentsRuntime::Bridge
{
	class IContentBridge;
}

namespace ContentsRuntime::Core
{
	class IContent;

	class FContentThread
	{
	public:
		FContentThread(IContent& content, Bridge::IContentBridge& bridge);
		~FContentThread();

		void Start();
		void Stop();
		SContentThreadStats GetStatsSnapshot();

		void EnqueueEnter(std::uint64_t sessionId);
		void EnqueueLeave(std::uint64_t sessionId);
		void EnqueuePacket(FOwnedPacketEnvelope&& packet);

	private:
		struct SImpl;
		std::unique_ptr<SImpl> m_impl;
	};
}
