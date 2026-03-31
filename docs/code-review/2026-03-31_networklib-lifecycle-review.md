# 코드 리뷰 노트

## 1. 작업 개요
- 작업명: `NetworkLib` 수명주기 및 동시성 구조 리뷰
- 관련 이슈/요청: MMORPG 서버 전환 전 `NetworkLib` 재사용 가능 범위 검토
- 대상 모듈/파일:
  `NetworkLib/CNetServer.cpp`
  `NetworkLib/CBaseContents.cpp`
  `NetworkLib/CContentsThread.cpp`
- 변경 목적: 현재 구조를 바로 리팩터링하기 전에, 세션 해제/콘텐츠 이동/스레드 실행 구조의 위험 지점을 근거 기반으로 정리한다.

## 2. 변경 요약
- 핵심 변경 1: 코드 변경 없이 `NetworkLib` 핵심 수명주기 흐름을 문서화했다.
- 핵심 변경 2: 세션 슬롯 관리, 콘텐츠 이동 시점 검증, 종료 루프의 위험 요소를 우선순위 후보로 정리했다.
- 핵심 변경 3: 확정된 사실과 아직 검증하지 못한 가설을 분리해 후속 리팩터링 기준점을 만들었다.

## 3. 리뷰 포인트
- 어떤 관점으로 봐야 하는지:
  IOCP 기반 세션 객체가 해제되는 시점과 콘텐츠 스레드가 같은 세션을 참조하는 시점이 충돌하지 않는지 봐야 한다.
- 특히 주의할 부분:
  `m_arrPSessions` 슬롯 재사용, `m_iIOCountAndRelease` 참조 카운트 계약, `MoveJobEnqueue()` 이후 지연 처리 구간
- 기존 동작과 달라진 점:
  코드 동작 변경은 없고, 이후 리팩터링 대상의 우선순위를 문서로 고정했다.

## 4. 근거
### 4-1. 코드 근거
- 호출 경로:
  접속 수락은 `CNetServer::WorkerThread()` -> `AcceptExCompleted()` -> `OnAccept()` -> 콘텐츠 `MoveJobEnqueue()` 순으로 이어진다. [`CNetServer.cpp`](D:\Project\ServerPortfolio\NetworkLib\CNetServer.cpp#L839), [`CNetServer.cpp`](D:\Project\ServerPortfolio\NetworkLib\CNetServer.cpp#L727), [`CBaseContents.cpp`](D:\Project\ServerPortfolio\NetworkLib\CBaseContents.cpp#L7)
- 영향 범위:
  세션 생성/해제, 패킷 수신 큐 처리, 콘텐츠 프레임 루프, 서버 종료 루프 전반에 걸쳐 영향이 있다.
- 관련 타입/락/스레드/수명주기:
  `CNetSession`은 `m_iIOCountAndRelease`와 `RELEASE_FLAG`로 해제 가능 시점을 조절한다. [`CNetServer.cpp`](D:\Project\ServerPortfolio\NetworkLib\CNetServer.cpp#L96), [`CNetServer.cpp`](D:\Project\ServerPortfolio\NetworkLib\CNetServer.cpp#L635)
  콘텐츠 스레드는 `CContentsThread`가 별도 타이머 큐를 돌며 `CBaseContents::ProcessMoveJob()`와 `ProcessRecvMsg()`를 호출한다. [`CContentsThread.cpp`](D:\Project\ServerPortfolio\NetworkLib\CContentsThread.cpp#L215), [`ContentsFrameTask.cpp`](D:\Project\ServerPortfolio\NetworkLib\ContentsFrameTask.cpp)

### 4-2. 로그/재현 근거
- 재현 절차:
  이번 리뷰에서는 정적 코드 분석만 수행했다.
- 관찰 로그:
  실행 로그는 수집하지 않았다.
- 측정값:
  측정값 없음

## 5. 위험 요소
- 동시성 위험:
  `CBaseContents::ProcessMoveJob()`는 큐에 들어온 `sessionId`로 다시 `m_arrPSessions[index]`를 가져온 뒤, 현재 슬롯의 세션 ID가 여전히 같은지 재검증하지 않고 `OnEnter()`와 `RegisterContents()`를 수행한다. [`CBaseContents.cpp`](D:\Project\ServerPortfolio\NetworkLib\CBaseContents.cpp#L43)
  `MoveJobEnqueue()` 시점에는 세션 참조 카운트를 올리지만, 실제 `ProcessMoveJob()` 실행은 이후 프레임이므로 그 사이에 슬롯이 재사용되면 잘못된 세션에 콘텐츠가 연결될 위험이 있다.
- 널/수명주기 위험:
  `ReleaseSession()`과 `ReleaseSessionPQCS()`는 세션을 반환한 뒤 `m_arrPSessions[index]`를 `nullptr`로 비우지 않는다. [`CNetServer.cpp`](D:\Project\ServerPortfolio\NetworkLib\CNetServer.cpp#L635), [`CNetServer.cpp`](D:\Project\ServerPortfolio\NetworkLib\CNetServer.cpp#L664)
  이후 `SendPacket()`, `Disconnect()`, `ProcessRecvMsg()` 등은 해당 배열 원소를 즉시 역참조하므로, 슬롯에 남은 포인터가 해제된 세션이거나 재사용 중인 세션이면 추적이 어려운 오류로 이어질 수 있다. [`CNetServer.cpp`](D:\Project\ServerPortfolio\NetworkLib\CNetServer.cpp#L491), [`CNetServer.cpp`](D:\Project\ServerPortfolio\NetworkLib\CNetServer.cpp#L590), [`CBaseContents.cpp`](D:\Project\ServerPortfolio\NetworkLib\CBaseContents.cpp#L86)
- 성능 영향:
  `CNetServer::Stop()`은 세션 카운트가 0이 될 때까지 sleep 없이 busy-wait 한다. [`CNetServer.cpp`](D:\Project\ServerPortfolio\NetworkLib\CNetServer.cpp#L464)
  세션 정리가 지연되거나 문제 상황에서 멈추면 CPU를 계속 점유한 채 종료 대기에 들어갈 수 있다.
- 롤백 필요 가능성:
  세션 수명주기 계약을 건드리는 리팩터링은 광범위한 회귀 위험이 있다.
  특히 `m_arrPSessions` 정리 방식과 `m_iIOCountAndRelease` 사용 규칙을 바꾸면 Accept/Recv/Send/Disconnect 전 경로를 함께 검증해야 한다.

## 6. 검증
- 빌드:
  아직 수행하지 못했다.
- 테스트:
  자동 테스트 없음
- 수동 확인:
  `CNetServer.cpp`, `CBaseContents.cpp`, `CContentsThread.cpp`를 기준으로 호출 흐름과 수명주기 경로를 정적 검토했다.
- 미검증 항목:
  실제 빌드 가능 여부
  더미 클라이언트 접속/종료 시 세션 슬롯 재사용 동작
  `CNetSession::Free()`와 TLS 메모리 풀 반환 이후 포인터 안정성
  종료 중 `Stop()`과 워커 스레드 간 실제 경쟁 조건

## 7. 결론
### 확정된 사실
- `NetworkLib`는 IOCP 기반 서버 루프, 세션 ID 발급, 콘텐츠 프레임 처리 구조를 이미 갖추고 있다.
- 세션 수명주기 제어는 `m_iIOCountAndRelease`와 `RELEASE_FLAG`에 강하게 의존한다.
- 콘텐츠 계층은 전역 서버와 세션 배열에 직접 접근하는 구조라 결합도가 높다.
- 서버 종료 루프는 현재 busy-wait 방식이다.

### 불확실한 부분
- `m_arrPSessions[index]`에 해제 후 포인터를 남겨두는 것이 현재 메모리 풀 구현과 함께 실제로 얼마나 안전한지는 추가 확인이 필요하다.
- `MoveJobEnqueue()`에서 증가시킨 참조 카운트가 지연 프레임 후 처리까지 충분히 안전성을 보장하는지는 실측 검증이 필요하다.
- `CContentsThread`의 Work Stealing/DelegateWork 로직이 실제 부하 상황에서 공정성과 지연 측면에서 적절한지는 아직 확인하지 못했다.

### 추가 확인 필요
- x64 Debug 기준 실제 빌드를 수행해 현재 기준선이 유효한지 확인할 것
- 세션 해제 시 `m_arrPSessions[index] = nullptr`가 빠져도 되는 구조인지 메모리 풀 구현까지 포함해 확인할 것
- 콘텐츠 이동 시 `sessionId` 재검증을 추가하는 방향이 맞는지 설계할 것
- `Stop()`의 busy-wait를 이벤트 기반 종료 대기로 바꾸는 방안을 검토할 것
- `remoteAddr->sin_port`를 그대로 저장하는 현재 코드가 의도인지, `ntohs()` 변환이 필요한지 확인할 것 [`CNetServer.cpp`](D:\Project\ServerPortfolio\NetworkLib\CNetServer.cpp#L756)
