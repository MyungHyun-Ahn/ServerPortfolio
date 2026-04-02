#include "Pch.h"

#include "ContentsRuntime/Core/FContentThread.h"

#include "ContentsRuntime/Bridge/IContentBridge.h"
#include "ContentsRuntime/Core/IContent.h"

namespace ContentsRuntime::Core
{
	struct FContentThread::SImpl
	{
		IContent* content = nullptr;
		Bridge::IContentBridge* bridge = nullptr;
		std::thread workerThread;
		std::mutex lock;
		std::condition_variable wakeCondition;
		std::deque<std::uint64_t> enterQueue;
		std::deque<std::uint64_t> leaveQueue;
		std::deque<FOwnedPacketEnvelope> packetQueue;
		bool running = false;
	};

	FContentThread::FContentThread(IContent& content, Bridge::IContentBridge& bridge)
		: m_impl(std::make_unique<SImpl>())
	{
		m_impl->content = &content;
		m_impl->bridge = &bridge;
	}

	FContentThread::~FContentThread()
	{
		Stop();
	}

	void FContentThread::Start()
	{
		if (m_impl->running)
		{
			return;
		}

		m_impl->running = true;
		m_impl->workerThread = std::thread([this]()
		{
			SImpl& impl = *m_impl;
			const std::uint32_t targetFps = std::max<std::uint32_t>(1u, impl.content->GetTargetFps());
			const auto frameDuration = std::chrono::milliseconds(std::max<std::int64_t>(1, 1000 / static_cast<std::int64_t>(targetFps)));
			auto nextFrameTime = std::chrono::steady_clock::now() + frameDuration;

			while (true)
			{
				std::deque<std::uint64_t> enterQueue;
				std::deque<std::uint64_t> leaveQueue;
				std::deque<FOwnedPacketEnvelope> packetQueue;
				int delayFrame = 1;
				bool shouldRunFrame = false;

				{
					std::unique_lock<std::mutex> lock(impl.lock);
					impl.wakeCondition.wait_until(
						lock,
						nextFrameTime,
						[&impl]()
						{
							return !impl.running ||
								!impl.enterQueue.empty() ||
								!impl.leaveQueue.empty() ||
								!impl.packetQueue.empty();
						});

					if (!impl.running)
					{
						break;
					}

					enterQueue.swap(impl.enterQueue);
					leaveQueue.swap(impl.leaveQueue);
					packetQueue.swap(impl.packetQueue);

					const auto now = std::chrono::steady_clock::now();
					if (now >= nextFrameTime)
					{
						const auto overdue = now - nextFrameTime;
						delayFrame = 1 + static_cast<int>(overdue / frameDuration);
						nextFrameTime += frameDuration * delayFrame;
						shouldRunFrame = true;
					}
				}

				for (const std::uint64_t sessionId : enterQueue)
				{
					impl.content->OnEnter(sessionId, *impl.bridge);
				}

				for (const std::uint64_t sessionId : leaveQueue)
				{
					impl.content->OnLeave(sessionId, *impl.bridge);
				}

				for (FOwnedPacketEnvelope& packet : packetQueue)
				{
					impl.content->OnPacket(
						packet.sessionId,
						packet.opcode,
						std::span<const char>(packet.payload.data(), packet.payload.size()),
						*impl.bridge);
				}

				if (shouldRunFrame)
				{
					impl.content->OnFrame(delayFrame, *impl.bridge);
				}
			}
		});
	}

	void FContentThread::Stop()
	{
		if (!m_impl->running)
		{
			return;
		}

		{
			std::lock_guard<std::mutex> lock(m_impl->lock);
			m_impl->running = false;
		}
		m_impl->wakeCondition.notify_all();

		if (m_impl->workerThread.joinable())
		{
			m_impl->workerThread.join();
		}
	}

	void FContentThread::EnqueueEnter(std::uint64_t sessionId)
	{
		{
			std::lock_guard<std::mutex> lock(m_impl->lock);
			m_impl->enterQueue.push_back(sessionId);
		}
		m_impl->wakeCondition.notify_one();
	}

	void FContentThread::EnqueueLeave(std::uint64_t sessionId)
	{
		{
			std::lock_guard<std::mutex> lock(m_impl->lock);
			m_impl->leaveQueue.push_back(sessionId);
		}
		m_impl->wakeCondition.notify_one();
	}

	void FContentThread::EnqueuePacket(FOwnedPacketEnvelope&& packet)
	{
		{
			std::lock_guard<std::mutex> lock(m_impl->lock);
			m_impl->packetQueue.push_back(std::move(packet));
		}
		m_impl->wakeCondition.notify_one();
	}
}
