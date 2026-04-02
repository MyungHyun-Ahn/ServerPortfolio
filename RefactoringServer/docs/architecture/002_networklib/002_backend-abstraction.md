# Backend Abstraction

## 1. 목적
- `IOCP`, `RIO`, `boost.asio`를 같은 상위 서버 코드에서 비교할 수 있게 한다.
- 상위 애플리케이션은 백엔드 구현체를 직접 알지 않고, `IServer`만 사용한다.

## 2. 현재 구조
### 2-1. 서버 인터페이스
- `IServer`
  - `Start(const SServerConfig&, IApplicationHandler&)`
  - `Stop()`
  - `Send(uint64_t sessionId, const char* buffer, int32_t length)`
  - `GetBackendKind()`

### 2-2. 애플리케이션 인터페이스
- `IApplicationHandler`
  - `OnServerStarted`
  - `OnClientConnected`
  - `OnPacketReceived`
  - `OnClientDisconnected`
  - `OnServerStopped`

### 2-3. 구현 선택
- 팩토리는 `EBackendKind`를 기준으로 구현체를 만든다.
- 현재 실제 구현은 `FIocpServer`뿐이다.
- 나머지는 스텁으로 두고, 상위 계층 경계를 먼저 고정한다.

## 3. 현재 장점
- `EchoServer`는 구현체 타입을 직접 모르고 시작할 수 있다.
- 나중에 `RIO`, `boost.asio`를 추가해도 상위 서버 코드 흔들림을 줄일 수 있다.
- 최소한의 비교 실험 구조를 초기에 확보했다.

## 4. 현재 한계
- `Send()` 경로는 아직 단순하다.
- 백엔드별 상세 설정 차이를 일반화하는 계층은 아직 없다.
- 테스트 기준도 아직 `echo 왕복 검증` 중심이다.

## 5. 다음 작업
- 백엔드별 공통 회귀 테스트 시나리오 정의
- 세션 핸들, 송신 큐, 프레이밍 경계가 추가된 뒤 인터페이스 재검토
