# NetworkLib Session Design

## 1. 목적
- 레거시 `CNetSession`의 장점을 참고해 `RefactoringServer/NetworkLib`용 세션 클래스를 설계한다.
- 현재 `FIocpServer` 내부의 `SSessionContext`를 장기적으로 대체할 수 있는 기준을 세운다.
- 네트워크 세션과 게임/콘텐츠 상태를 분리하는 원칙을 고정한다.

## 2. 레거시에서 확인한 사실

### 2-1. 참고 대상
- [CNetServer.h](D:\Project\ServerPortfolio\includes\NetworkLib\CNetServer.h)
- [CNetServer.cpp](D:\Project\ServerPortfolio\NetworkLib\CNetServer.cpp)

### 2-2. 레거시 `CNetSession`이 맡고 있던 책임
- 소켓 보관
- 세션 ID 보관
- recv 링버퍼
- send queue
- recv message queue
- overlapped 메모리 시작 주소 관리
- send 상태 플래그
- I/O 수명주기와 release 플래그 관리
- 현재 콘텐츠 포인터 보관

### 2-3. 레거시에서 배울 점
- 세션이 “연결 단위의 상태 묶음”이라는 점은 맞다.
- recv/send/I/O 참조 카운트가 세션 경계에 붙어 있어야 수명주기 관리가 쉽다.
- 세션 ID와 소켓, recv buffer는 분리하기보다 같은 객체에 모여 있는 편이 자연스럽다.

### 2-4. 레거시에서 그대로 가져오지 않을 점
- 콘텐츠 포인터를 세션 내부에 직접 두는 구조
- send queue와 recv message queue를 한 세션에 과도하게 몰아넣은 구조
- 전역 서버와 강결합된 호출
- TLS pool, overlapped allocator, profiling 매크로까지 세션 클래스에 바로 엮인 구조

## 3. 현재 `RefactoringServer` 상태
- [FIocpServer.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FIocpServer.h)의 `SSessionContext`가 현재 임시 세션 역할을 한다.
- 현재 들어 있는 상태는 아래 정도다.
  - `socket`
  - `sessionId`
  - `slotIndex`
  - `generation`
  - `refCount`
  - `closing`
  - `recvContext`
  - `recvBuffer`
- 즉 지금은 “최소 수명주기와 recv 누적 버퍼”만 있는 단계다.

## 4. 새 세션 설계 원칙

### 4-1. 네트워크 세션과 콘텐츠 세션은 분리
- `NetworkLib`의 세션은 네트워크 계층 책임만 가진다.
- 계정 번호, 로그인 상태, 플레이어 객체 포인터 같은 정보는 세션 내부에 두지 않는다.
- 그런 정보는 나중에 콘텐츠/게임 계층의 `PlayerSessionContext` 또는 `ConnectionState` 쪽으로 분리한다.

### 4-2. 세션은 서버 내부 구현 객체
- 처음부터 외부에 공개되는 무거운 `ISession` 인터페이스보다는, `NetworkLib` 내부 구현 클래스 `FSession`으로 시작하는 편이 낫다.
- 외부에 세션을 노출할 필요가 생기면 그때 읽기 전용 핸들이나 뷰를 추가한다.

### 4-3. 책임은 “연결 단위 상태”로 제한
- 세션 클래스가 가져야 할 핵심 책임
  - 소켓
  - 세션 ID
  - generation / slot index
  - recv 누적 버퍼
  - I/O ref count
  - closing 상태
  - recv/send I/O context
- 세션 클래스가 직접 가지지 않을 책임
  - 게임 로직 상태
  - opcode dispatch
  - 콘텐츠 객체 수명주기
  - 전역 설정 로딩

## 5. 1차 `FSession` 제안

### 5-1. 위치
- `RefactoringServer/NetworkLib/Servers/FSession.h`
- `RefactoringServer/NetworkLib/Servers/FSession.cpp`

### 5-2. 1차 필드 후보
- `SOCKET m_socket`
- `std::uint64_t m_sessionId`
- `std::uint32_t m_slotIndex`
- `std::uint32_t m_generation`
- `std::atomic<long> m_refCount`
- `std::atomic<bool> m_closing`
- `std::vector<char> m_recvBuffer`
- `SIoContext m_recvContext`
- `SIoContext m_sendContext` 또는 추후 send queue 연결 지점

### 5-3. 1차 메서드 후보
- `Initialize(...)`
- `Reset()`
- `AppendRecvBytes(...)`
- `AcquireRef()`
- `ReleaseRef()`
- `MarkClosing()`
- `IsClosing()`

## 6. 콘텐츠 계층과의 분리 기준
- 레거시의 [CPlayerSessionContext.h](D:\Project\ServerPortfolio\WorldServer\CPlayerSessionContext.h)처럼
  - `accountNo`
  - `isLoggedIn`
  - `prevRecvTime`
  같은 정보는 네트워크 세션이 아니라 상위 계층 컨텍스트가 가져가는 편이 맞다.
- 즉 장기 구조는 아래처럼 나눈다.
  - `NetworkLib::FSession`
  - `WorldServer::FPlayerSessionContext` 또는 이와 유사한 콘텐츠 컨텍스트

## 7. 구현 순서 제안
1. `SSessionContext`를 직접 확장하지 말고 `FSession` 클래스를 새로 만든다.
2. `FIocpServer` 내부에서 `SSessionContext`를 `FSession`으로 교체한다.
3. 기존 동작이 깨지지 않게 recv/send, refCount, closing 경계만 먼저 이전한다.
4. 그 다음 send queue, last recv tick, session statistics 같은 보조 정보가 필요하면 단계적으로 추가한다.

## 8. 검증 기준
- `EchoServer` / `EchoClient` 기본 왕복 성공
- 다중 요청, 분할 송신, 누적 수신 검증 성공
- 세션 connect/disconnect 로그 정상
- 기존 `LockFreeTests` 회귀에 영향 없음

## 9. 결론
- 세션 클래스는 필요하다.
- 다만 레거시처럼 콘텐츠 상태까지 끌어안는 세션이 아니라, 네트워크 계층 책임만 가진 `FSession`으로 시작하는 것이 맞다.
- 이 방향이 이후 송신 큐, 세션 통계, 콘텐츠 연결, 플레이어 컨텍스트 분리까지 가장 자연스럽게 이어진다.
