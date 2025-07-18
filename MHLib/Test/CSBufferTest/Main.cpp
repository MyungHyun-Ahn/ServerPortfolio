
#include "MHLib/utils/CProfileManager.h"
#include "MHLib/utils/CSmartPtr.h"
#include "MHLib/memory/CTLSMemoryPool.h"
#include "MHLib/memory/CTLSPagePool.h"
#include "CRingBuffer.h"
#include "CRecvBuffer.h"
#include "CSerializableBuffer.h"
#include "CSerializableBufferView.h"

constexpr LONG DATA_SIZE = 64;
constexpr LONG DATA_COUNT = 200;

constexpr LONG LOOP_COUNT = 100000;

#pragma pack(push, 1)
struct LanHeader
{
	USHORT len;
};
#pragma pack(pop)

void ProviderRing(CRingBuffer &m_RecvBuffer)
{
	for (int i = 0; i < DATA_COUNT; i++)
	{
		LanHeader lanHeader;
		lanHeader.len = DATA_SIZE;
		m_RecvBuffer.Enqueue((char *)&lanHeader, sizeof(LanHeader));

		char data[DATA_SIZE];
		m_RecvBuffer.Enqueue(data, DATA_SIZE);
	}
}

void RecvCompletedRing(CRingBuffer &m_RecvBuffer)
{
	int currentUseSize = m_RecvBuffer.GetUseSize();
	while (currentUseSize > 0)
	{
		if (currentUseSize < sizeof(LanHeader))
			break;

		LanHeader packetHeader;
		m_RecvBuffer.Peek((char *)&packetHeader, sizeof(LanHeader));
		CSerializableBuffer<TRUE> *view = CSerializableBuffer<TRUE>::Alloc();
		m_RecvBuffer.MoveFront(sizeof(LanHeader));
		view->EnqueueHeader((char *)&packetHeader, sizeof(LanHeader));

		m_RecvBuffer.Dequeue(view->GetContentBufferPtr(), packetHeader.len);
		view->MoveWritePos(packetHeader.len);
		view->IncreaseRef();
		// OnRecv();
		if (view->DecreaseRef() == 0)
			CSerializableBuffer<TRUE>::Free(view);
		currentUseSize = m_RecvBuffer.GetUseSize();
	}
}

void ProviderView(CRecvBuffer *m_RecvBuffer)
{
	for (int i = 0; i < DATA_COUNT; i++)
	{
		LanHeader lanHeader;
		lanHeader.len = DATA_SIZE;
		m_RecvBuffer->Enqueue((char *)&lanHeader, sizeof(LanHeader));

		char data[DATA_SIZE];
		m_RecvBuffer->Enqueue(data, sizeof(char) * DATA_SIZE);
	}
}

void RecvCompletedView(CRecvBuffer *m_pRecvBuffer)
{
	bool delayFlag = false;

	DWORD currentUseSize = m_pRecvBuffer->GetUseSize();
	while (currentUseSize > 0)
	{
		LanHeader packetHeader;
		CSerializableBufferView<TRUE> *view = CSerializableBufferView<TRUE>::Alloc();

		if (currentUseSize < (int)CSerializableBuffer<TRUE>::DEFINE::HEADER_SIZE)
		{
			// Peek 실패 delayedBuffer로
			delayFlag = true;

			int remainSize = m_pRecvBuffer->GetUseSize();
			// 남은 사이즈 만큼 씀
			view->WriteDelayedHeader(m_pRecvBuffer->GetFrontPtr(), remainSize);
			m_pRecvBuffer->MoveFront(remainSize);
			// m_pDelayedBuffer = view;

			// g_Logger->WriteLog(L"RecvBuffer", LOG_LEVEL::DEBUG, L"HeaderDelay");
			break;
		}

		int useSize = m_pRecvBuffer->GetUseSize();
		m_pRecvBuffer->Peek((char *)&packetHeader, (int)CSerializableBuffer<TRUE>::DEFINE::HEADER_SIZE);

		// 실질적으로 Recv 버퍼 입장에서는 읽은 것
		// - 하나의 메시지가 완전히 완성된 상태
		int offsetStart = m_pRecvBuffer->GetFrontOffset();
		int offsetEnd = offsetStart + packetHeader.len + (int)CSerializableBuffer<TRUE>::DEFINE::HEADER_SIZE;

		// 이 view를 그냥 써도 됨
		// Init 에서 RecvBuffer의 Ref가 증가
		view->Init(m_pRecvBuffer, offsetStart, offsetEnd);
		m_pRecvBuffer->MoveFront((int)CSerializableBuffer<TRUE>::DEFINE::HEADER_SIZE + packetHeader.len);

		// g_NetServer->OnRecv(m_uiSessionID, view);
		view->IncreaseRef();
		if (view->DecreaseRef() == 0)
			CSerializableBufferView<TRUE>::Free(view);

		currentUseSize = m_pRecvBuffer->GetUseSize();
	}

	// Ref 감소
	if (m_pRecvBuffer->DecreaseRef() == 0)
		CRecvBuffer::Free(m_pRecvBuffer);

	m_pRecvBuffer = nullptr;
}

// 헤더 만들고 데이터 만들고 -> 버퍼 생성
// RecvCompleted 완료 시간 측정
// 데이터 사이즈에 따른 완료 시간 측정
int main()
{
	MHLib::utils::g_ProfileMgr = MHLib::utils::CProfileManager::GetInstance();
	int ringBufferSize = 102400 * 2;


	CRingBuffer ringBuffer(ringBufferSize);
	for (int i = 0; i < LOOP_COUNT; i++)
	{
		ProviderRing(ringBuffer);
		{
			PROFILE_BEGIN(0, "Ring");
			RecvCompletedRing(ringBuffer);
		}
	}

	for (int i = 0; i < LOOP_COUNT; i++)
	{
		CRecvBuffer *pRecvBuffer = CRecvBuffer::Alloc();
		ProviderView(pRecvBuffer);
		pRecvBuffer->IncreaseRef();
		{
			PROFILE_BEGIN(0, "View");
			RecvCompletedView(pRecvBuffer);
		}
	}

	MHLib::utils::g_ProfileMgr->DataOutToFileWithTag(DATA_SIZE);
	return 0;
}