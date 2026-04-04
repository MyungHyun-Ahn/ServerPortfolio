# 로비/룸 멀티 인스턴스 흐름 리뷰

## 1. 목적
- `ContentsRuntime`의 멀티 인스턴스 라우팅 위에 실제 사용자 흐름을 올려서 검증한다.
- 대상 흐름은 다음과 같다.
  - `Login -> Lobby -> RoomList -> RoomEnter -> RoomEcho`
  - `RoomEcho` 중 `RoomList -> RoomChange -> RoomEcho`

## 2. 이번 변경 핵심
- 콘텐츠 흐름
  - `AuthContent`는 로그인 성공 후 `LobbyContent`로 보낸다.
  - `LobbyContent`는 룸 목록 조회와 룸 입장을 처리한다.
  - `RoomContent`는 에코 요청과 룸 변경을 처리한다.
- 라우팅
  - 실제 이동은 `MoveSessionToInstance(sessionId, contentInstanceId)` 기준으로 수행한다.
  - 클라이언트가 보는 식별자는 `roomId`이고, 서버는 내부에서 `roomId -> contentInstanceId`를 해석한다.
- 룸 상태
  - `FRoomRegistry`가 룸 목록, 정원, 현재 인원, 세션-룸 매핑을 관리한다.

## 3. 실패 코드와 로그 정책
- `RoomEnterRp`, `RoomChangeRp`에는 `resultCode`가 들어간다.
- 정상 실패 예시
  - `RoomFull`
  - `SameRoomNotAllowed`
  - `RetryRequired`
- 비정상 실패 예시
  - `MissingContentInstance`
  - `RuntimeRouteFailure`
  - `InternalError`
- 정상 실패는 `Info` 수준으로 남기고, 비정상 실패는 `Error`로 남긴다.

## 4. 중요한 규칙
- `RoomChangeRq` 전에는 항상 최신 `RoomListRq/Rp`로 유효한 룸 목록을 다시 받아야 한다.
- 전이 응답은 `MoveSession` 완료 후 전송해야 한다.
- 룸 정원 초과 같은 정상 실패는 테스트 실패로 보지 않고 재시도 대상으로 본다.
- 없는 인스턴스나 런타임 라우팅 실패는 비정상으로 보고 즉시 실패 처리한다.

## 5. 클라이언트 동작
- 연결 직후
  - `LoginRq`
  - `RoomListRq`
  - 랜덤 joinable room 선택
  - `RoomEnterRq`
- 룸 안에서는
  - `EchoRq`
  - 확률적으로 `RoomListRq`
  - 다른 joinable room이 있으면 `RoomChangeRq`
- 정상 실패를 받으면 최신 `RoomListRq` 기준으로 다시 시도한다.

## 6. 검증 결과
- 패킷 재생성 성공
- 솔루션 x64 Debug 빌드 성공
- [LockFreeTests.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\LockFreeTests.exe) 통과
- 짧은 런타임 스모크 성공
  - 서버 로그: [server.log](D:\Project\ServerPortfolio\RefactoringServer\Out\roomflow_smoke\server.log)
  - 클라이언트 로그: [client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\roomflow_smoke\client.log)
- send 재기동 보장 수정 후 무timeout 6시간 RTT 런 정상 종료
  - 클라이언트 로그: [client_20260404_041225.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rtt_no_timeout_6h_sendfix\client_20260404_041225.log)
  - RTT CSV: [rtt_20260404_041225.csv](D:\Project\ServerPortfolio\RefactoringServer\Out\rtt_no_timeout_6h_sendfix\rtt_20260404_041225.csv)

## 7. 런타임에서 확인된 점
- `roomId=77/78/79`, `contentInstanceId=3001/3002/3003`로 실제 분산이 일어났다.
- 같은 세션이 여러 룸 인스턴스 사이를 이동하는 로그가 확인되었다.
- 짧은 스모크에서는 정상 종료까지 포함해 문제가 없었다.

## 8. 남은 확인 항목
- 비정상 실패 코드가 실제로 어떤 운영 로그 패턴을 만드는지 추가 확인 필요
- `Lobby`, `Room` 외 다른 콘텐츠 타입을 붙였을 때 인스턴스 배치 정책을 더 구체화할 필요가 있다.
