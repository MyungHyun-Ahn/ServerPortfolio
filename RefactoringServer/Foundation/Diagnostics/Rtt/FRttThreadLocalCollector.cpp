#include "Pch.h"

#include "FRttThreadLocalCollector.h"

#include "FRttMetricsRuntime.h"

namespace
{
	struct STlsRttState
	{
		Foundation::Diagnostics::FRttMetricsRuntime* rttMetricsRuntime = nullptr;
		std::int64_t activeBucketStartEpochSeconds = 0;
		bool hasActiveBucket = false;
		std::vector<Foundation::Diagnostics::SRttStageAggregate> stageAggregates;
	};

	thread_local STlsRttState g_tlsRttState{};

	std::int64_t ToEpochMilliseconds(const std::chrono::system_clock::time_point timePoint)
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(timePoint.time_since_epoch()).count();
	}

	std::int64_t ToBucketStartEpochSeconds(const std::chrono::system_clock::time_point timePoint, const int intervalSeconds)
	{
		const std::int64_t epochSeconds =
			std::chrono::duration_cast<std::chrono::seconds>(timePoint.time_since_epoch()).count();
		return epochSeconds - (epochSeconds % std::max(1, intervalSeconds));
	}

	void InsertTopSample(
		std::array<Foundation::Diagnostics::SRttTopSample, 3>& topSamples,
		const Foundation::Diagnostics::SRttTopSample& sample)
	{
		if (sample.rttMs <= 0.0)
		{
			return;
		}

		for (std::size_t sampleIndex = 0; sampleIndex < topSamples.size(); ++sampleIndex)
		{
			if (sample.rttMs > topSamples[sampleIndex].rttMs)
			{
				for (std::size_t moveIndex = topSamples.size() - 1; moveIndex > sampleIndex; --moveIndex)
				{
					topSamples[moveIndex] = topSamples[moveIndex - 1];
				}

				topSamples[sampleIndex] = sample;
				return;
			}
		}
	}

	void ResetStageAggregates(std::vector<Foundation::Diagnostics::SRttStageAggregate>& stageAggregates)
	{
		for (Foundation::Diagnostics::SRttStageAggregate& stageAggregate : stageAggregates)
		{
			stageAggregate = Foundation::Diagnostics::SRttStageAggregate{};
		}
	}

	void FlushTlsSnapshot()
	{
		if (g_tlsRttState.rttMetricsRuntime == nullptr || !g_tlsRttState.hasActiveBucket)
		{
			return;
		}

		auto snapshot = std::make_unique<Foundation::Diagnostics::SRttSnapshot>();
		snapshot->bucketStartEpochSeconds = g_tlsRttState.activeBucketStartEpochSeconds;
		snapshot->stageAggregates = g_tlsRttState.stageAggregates;
		g_tlsRttState.rttMetricsRuntime->EnqueueSnapshot(std::move(snapshot));

		ResetStageAggregates(g_tlsRttState.stageAggregates);
		g_tlsRttState.activeBucketStartEpochSeconds = 0;
		g_tlsRttState.hasActiveBucket = false;
	}

	void EnsureTlsBucket(const std::chrono::system_clock::time_point nowSystem)
	{
		if (g_tlsRttState.rttMetricsRuntime == nullptr)
		{
			return;
		}

		const std::int64_t bucketStartEpochSeconds =
			ToBucketStartEpochSeconds(nowSystem, g_tlsRttState.rttMetricsRuntime->GetFlushIntervalSeconds());
		if (!g_tlsRttState.hasActiveBucket)
		{
			g_tlsRttState.hasActiveBucket = true;
			g_tlsRttState.activeBucketStartEpochSeconds = bucketStartEpochSeconds;
			return;
		}

		if (g_tlsRttState.activeBucketStartEpochSeconds != bucketStartEpochSeconds)
		{
			FlushTlsSnapshot();
			g_tlsRttState.hasActiveBucket = true;
			g_tlsRttState.activeBucketStartEpochSeconds = bucketStartEpochSeconds;
		}
	}
}

namespace Foundation::Diagnostics
{
	FRttThreadLocalCollector::FRttThreadLocalCollector(FRttMetricsRuntime* const rttMetricsRuntime)
		: m_rttMetricsRuntime(rttMetricsRuntime)
	{
		if (m_rttMetricsRuntime == nullptr)
		{
			return;
		}

		g_tlsRttState.rttMetricsRuntime = m_rttMetricsRuntime;
		g_tlsRttState.activeBucketStartEpochSeconds = 0;
		g_tlsRttState.hasActiveBucket = false;
		g_tlsRttState.stageAggregates.assign(m_rttMetricsRuntime->GetStageCount(), SRttStageAggregate{});
	}

	FRttThreadLocalCollector::~FRttThreadLocalCollector()
	{
		if (g_tlsRttState.rttMetricsRuntime != m_rttMetricsRuntime)
		{
			return;
		}

		FlushTlsSnapshot();
		g_tlsRttState = STlsRttState{};
	}

	SRttPendingRequest FRttThreadLocalCollector::BeginRequest(const FRttStageIndex stageIndex, const int sessionIndex) const
	{
		SRttPendingRequest pendingRequest{};
		pendingRequest.stageIndex = stageIndex;
		pendingRequest.sessionIndex = sessionIndex;
		pendingRequest.sentSteady = std::chrono::steady_clock::now();
		pendingRequest.sentSystem = std::chrono::system_clock::now();
		return pendingRequest;
	}

	void FRttThreadLocalCollector::RecordSample(
		const SRttPendingRequest& pendingRequest,
		const std::chrono::system_clock::time_point receivedSystem) const
	{
		if (m_rttMetricsRuntime == nullptr ||
			g_tlsRttState.rttMetricsRuntime != m_rttMetricsRuntime ||
			!m_rttMetricsRuntime->IsStageIndexValid(pendingRequest.stageIndex))
		{
			return;
		}

		EnsureTlsBucket(receivedSystem);
		if (!g_tlsRttState.hasActiveBucket)
		{
			return;
		}

		const auto receivedSteady = std::chrono::steady_clock::now();
		const double rttMs =
			static_cast<double>(
				std::chrono::duration_cast<std::chrono::microseconds>(receivedSteady - pendingRequest.sentSteady).count()) /
			1000.0;

		SRttStageAggregate& stageAggregate =
			g_tlsRttState.stageAggregates[static_cast<std::size_t>(pendingRequest.stageIndex)];
		++stageAggregate.sampleCount;
		stageAggregate.totalRttMs += rttMs;

		SRttTopSample sample{};
		sample.rttMs = rttMs;
		sample.sessionIndex = pendingRequest.sessionIndex;
		sample.sentEpochMs = ToEpochMilliseconds(pendingRequest.sentSystem);
		sample.recvEpochMs = ToEpochMilliseconds(receivedSystem);
		InsertTopSample(stageAggregate.topSamples, sample);
	}

	void FRttThreadLocalCollector::RecordTimeout(
		const FRttStageIndex stageIndex,
		const std::chrono::system_clock::time_point timeoutSystem) const
	{
		if (m_rttMetricsRuntime == nullptr ||
			g_tlsRttState.rttMetricsRuntime != m_rttMetricsRuntime ||
			!m_rttMetricsRuntime->IsStageIndexValid(stageIndex))
		{
			return;
		}

		EnsureTlsBucket(timeoutSystem);
		if (!g_tlsRttState.hasActiveBucket)
		{
			return;
		}

		SRttStageAggregate& stageAggregate = g_tlsRttState.stageAggregates[static_cast<std::size_t>(stageIndex)];
		++stageAggregate.timeoutCount;
	}
}
