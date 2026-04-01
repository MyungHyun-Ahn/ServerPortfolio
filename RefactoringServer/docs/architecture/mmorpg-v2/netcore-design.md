# 기획 노트

## 1. 배경
- 2세대 구조에서는 네트워크 코어를 독립 라이브러리로 먼저 설계하고, 그 위에 에코 서버와 이후 MMORPG 서버를 쌓아 올릴 계획이다.
- 백엔드 비교 요구사항이 있으므로 `IOCP`, `RIO`, `boost.asio`를 같은 상위 인터페이스 아래에 두고 테스트 가능해야 한다.

## 2. 목표
- `NetCore` 라이브러리에서 백엔드별 차이를 숨기는 최소 공통 인터페이스를 정의한다.
- 첫 번째 구현은 `IOCP`만 넣고, `RIO`, `boost.asio`는 팩토리와 타입 수준에서 확장 지점을 남긴다.
- `EchoServer`와 `EchoClient`로 실제 빌드와 기초 검증이 가능한 상태를 만든다.

## 3. 비목표
- 이번 단계에서 `RIO`와 `boost.asio` 실제 구현까지 넣지 않는다.
- 패킷 프레이밍, 암호화, 게임 프로토콜은 아직 넣지 않는다.
- 월드 상태나 플레이어 모델은 포함하지 않는다.

## 4. 공통 인터페이스
### 4-1. `ITransportServer`
- `Start(const SServerConfig&, IApplicationHandler&)`
- `Stop()`
- `Send(uint64_t sessionId, const char* buffer, int32_t length)`
- `GetBackendKind()`

### 4-2. `IApplicationHandler`
- `OnServerStarted`
- `OnClientConnected`
- `OnPacketReceived`
- `OnClientDisconnected`
- `OnServerStopped`

### 4-3. 구성 데이터
- `SServerConfig`
  - backend kind
  - bind ip
  - port
  - worker thread count
  - max session count
  - recv buffer size

## 5. 백엔드 분리 전략
- 팩토리에서 `EBackendKind`를 기준으로 구현체를 생성한다.
- `IOCP` 구현은 실제 동작한다.
- `RIO`, `boost.asio` 구현은 현재 단계에서 `Start()` 시 unsupported를 반환하는 스텁이다.
- 상위 애플리케이션은 구현체 타입을 모르고 인터페이스만 사용한다.

## 6. 첫 번째 에코 테스트 범위
- `EchoServer`
  - `IOCP` 백엔드로 서버 시작
  - 접속한 클라이언트의 메시지를 그대로 돌려준다.
- `EchoClient`
  - 서버에 접속
  - 문자열 1회 송신
  - 동일 응답 수신 시 성공

## 7. 후속 확장 포인트
- 패킷 프레이밍 계층 분리
- 세션 핸들 정책 강화
- outbound queue와 back-pressure 도입
- `RIO`와 `boost.asio` 구현 추가
