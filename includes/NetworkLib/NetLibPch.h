#pragma once

#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm")
#pragma comment(lib, "DbgHelp.Lib")
#pragma comment(lib, "Pdh.lib")
#pragma comment(lib, "Synchronization.lib")

// 소켓 관련
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <MSWSock.h>

// 윈도우 API
#include <windows.h>
#include <Psapi.h> // 프로세스 메모리 pmi
#include <strsafe.h>
#include <Pdh.h>

// C 런타임 라이브러리
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <process.h>
#include <cwchar>

// 컨테이너 자료구조
#include <deque>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <string>
#include <queue>

// 기타 - 언어설정 등
#include <new>
#include <locale>
#include <DbgHelp.h>
#include <chrono>
#include <functional>


// MHLib
// - utils
#include "MHLib/utils/CLockGuard.h"
#include "MHLib/utils/DefineSingleton.h"
#include "MHLib/utils/CProfileManager.h"
#include "MHLib/utils/CSmartPtr.h"
#include "MHLib/utils/CLogger.h"
#include "MHLib/utils/CFileLoader.h"
#include "MHLib/utils/CMonitoringManager.h"

// - debug
#include "MHLib/debug/CCrashDump.h"

// - security
#include "MHLib/security/CEncryption.h"

// - Lock-Free
#include "MHLib/containers/CLFStack.h"
#include "MHLib/containers/CLFQueue.h"
#include "MHLib/memory/CTLSMemoryPool.h"
#include "MHLib/memory/CTLSPagePool.h"

// NetLib
// - Define
#include "NetLibDefine.h"

// - DataStructures
#include "COverlappedAllocator.h"
#include "CRingBuffer.h"
#include "CRecvBuffer.h"
#include "CSerializableBuffer.h"
#include "CSerializableBufferView.h"

// - Task
#include "BaseTask.h"