# ChattingServer Packet and Flow Plan

## 1. 목적
- `RefactoringServer` 라이브러리 기반 `ChattingServer`의 1차 패킷 계약과 서버/클라이언트 흐름을 고정한다.
- 기존 [001_chattingserver-large-packet-benchmark.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\009_chatting_server\001_chattingserver-large-packet-benchmark.md)에서 잡아둔 큰 패킷 benchmark 목표를 실제 구현 가능한 최소 packet surface로 구체화한다.
- 큰 payload 전송 시 `RIO Direct`와 `IOCP`를 같은 workload로 비교할 수 있는 재현 가능한 chat 시나리오를 만든다.

## 2. 범위
대상:
- `LoginRq/Rp`
- `RoomListRq/Rp`
- `RoomChangeRq/Rp`
- `ChattingRq/Rp`
- `Broadcast`
- `ChattingServer / ChatClient` 최소 검증 흐름

비범위:
- 친구, 귓속말, 공지
- 메시지 영속 저장
- 인증 고도화와 계정 시스템
- moderation, profanity filtering, read receipt
- 파일 첨부

## 3. 핵심 결정
- 1차 범위에서는 별도 `RoomEnterRq/Rp`를 두지 않는다.
- 첫 room 진입과 room 이동은 모두 `RoomChangeRq/Rp`로 처리한다.
- `Broadcast`는 같은 room 참여자 대상 fan-out이며 송신자 본인에게는 보내지 않는다.
- `ChattingRp`는 성공/실패 여부만 내려주는 최소 ack로 유지한다.
- 큰 패킷 비교가 목적이므로 채팅 payload는 `string`보다 `bytes` 기준으로 설계한다.
- `ChattingRp`가 성공/실패만 가지므로, 1차 클라이언트는 세션당 동시에 하나의 `ChattingRq`만 outstanding 상태로 유지한다.

## 4. 패킷 계약

### 4.1 Login
- `LoginRq/Rp`는 기존 로그인 흐름을 재사용한다.
- 로그인 성공 후 세션은 `Lobby` 또는 `NoRoom` 상태가 된다.
- 로그인 직후 클라이언트는 `RoomListRq`를 보낼 수 있어야 한다.

### 4.2 RoomList
- `RoomListRq`
  - body 없는 요청으로 유지한다.
- `RoomListRp`
  - 현재 선택 가능한 room 목록을 내려준다.
  - 최소 포함 정보:
    - `roomId`
    - `roomName`
    - `participantCount`
    - `capacity`
    - `joinable`
- 기존 [Chat.yaml](D:\Project\ServerPortfolio\RefactoringServer\Packet\Chat\Chat.yaml)의 room 목록 응답 형식을 최대한 재사용한다.

### 4.3 RoomChange
- `RoomChangeRq`
  - `targetRoomId : uint32`
- `RoomChangeRp`
  - `previousRoomId : uint32`
  - `currentRoomId : uint32`
  - `success : bool`
  - 필요 시 `resultCode : uint16` 유지 가능
- 규칙:
  - 첫 room 진입도 `RoomChangeRq`로 처리한다.
  - 아직 room에 속하지 않은 상태라면 `previousRoomId = 0`을 사용한다.
  - 같은 room으로의 이동 요청은 실패 처리한다.
  - 가득 찬 room, 유효하지 않은 room, 더 이상 존재하지 않는 room 대상 요청은 실패 처리한다.

### 4.4 Chatting
- `ChattingRq`
  - `roomId : uint32`
  - `payload : bytes`
  - 선택 필드:
    - `clientMessageId : uint64`
    - `sentTick : uint64`
- `ChattingRp`
  - `success : bool`
- 규칙:
  - 클라이언트는 현재 자신이 속한 room으로만 `ChattingRq`를 보낼 수 있다.
  - 서버는 room 소속 여부와 payload 크기 정책을 먼저 검증한다.
  - `ChattingRp(success=false)`인 경우 `Broadcast`는 발생하지 않는다.
  - `ChattingRp`가 성공/실패만 가지므로, 1차 클라이언트는 request-response 순서를 보장하도록 단일 outstanding 정책을 사용한다.

### 4.5 Broadcast
- `Broadcast`
  - 서버 origin one-way packet이다.
  - room fan-out 전용이다.
  - 최소 포함 정보:
    - `roomId : uint32`
    - `senderUserId : uint64`
    - `messageId : uint64`
    - `payload : bytes`
    - 필요 시 `sentTick : uint64`
- 규칙:
  - 송신자 본인에게는 보내지 않는다.
  - 같은 room의 다른 참여자들에게만 전송한다.
  - room에 송신자밖에 없으면 `Broadcast` 전송 횟수는 `0`일 수 있다.
- schema 표현은 [003_packet-generator-broadcast-contract.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\006_packet-schema-tooling\003_packet-generator-broadcast-contract.md)의 `broadcast` 계약을 따른다.

## 5. 서버/클라이언트 흐름
1. 클라이언트 연결
2. `LoginRq/Rp`
3. `RoomListRq/Rp`
4. 클라이언트는 목록에서 대상 room을 선택하고 `RoomChangeRq` 전송
5. 서버는 room 이동 성공 시 `RoomChangeRp(success=true)` 전송
6. 클라이언트는 현재 room으로 `ChattingRq` 전송
7. 서버는 먼저 `ChattingRp(success=true)` 전송
8. 서버는 같은 room의 다른 참여자들에게만 `Broadcast` fan-out
9. 클라이언트는 일정 확률로 `RoomListRq/Rp`를 다시 수행하고 다른 room으로 `RoomChangeRq`

추가 규칙:
- 로그인 직후 첫 room 선택도 `RoomChange` 흐름으로 통일한다.
- `ChattingRq`를 보낸 세션은 자기 자신의 `Broadcast`를 기다리지 않는다.
- `RoomChange` 실패는 콘텐츠 수준 거부이며 transport 오류로 취급하지 않는다.

## 6. 성능 비교 관점에서의 의미
- 송신자 제외 `Broadcast` 정책이므로 RTT는 `ChattingRq -> ChattingRp` 기준으로 본다.
- broadcast fan-out 성능은 `Broadcast delivered count`와 전송 바이트 수로 따로 본다.
- 동일 payload를 여러 세션에 fan-out하는 패턴이 `RIO Direct`의 send ring 사용량, stall, batching 효율을 드러내는 핵심 workload가 된다.
- 권장 payload 프로필:
  - `1 KiB`
  - `2 KiB`
  - `4 KiB`
  - `8 KiB` 근접

## 7. 제약과 리스크
- `ChattingRp`가 `success`만 가지면 여러 `ChattingRq`를 동시에 outstanding 상태로 둘 수 없다.
- 추후 세션당 병렬 send를 열고 싶으면 `ChattingRp`에 `clientMessageId` 같은 상관 키를 추가해야 한다.
- 별도 `RoomEnter`가 없으므로 `NoRoom -> Room` 전이와 `Room -> Room` 전이를 모두 `RoomChange` 한 경로에서 안전하게 처리해야 한다.
- 송신자 제외 `Broadcast` 정책에서는 room 인원이 `1`명일 때 fan-out 측정이 불가능하다.
- 오래된 `RoomListRp` 기준으로 `RoomChangeRq`가 들어오면 정상 실패 응답으로 처리해야 한다.

## 8. 구현 순서
1. `ChattingServer` 1차 packet contract 확정
2. `Chat.yaml` 정리 또는 전용 yaml 분리
3. `broadcast` schema/generator 경로 반영
4. generated packet build 확인
5. `ChattingServer` room membership / room change / chatting 처리 구현
6. `ChatClient` 검증 로직 구현
7. small payload smoke
8. large payload high-load benchmark

## 9. 결론
- `ChattingServer` 1차 범위는 `Login`, `RoomList`, `RoomChange`, `Chatting`, `Broadcast` 다섯 흐름으로 고정한다.
- 초기 room 진입까지 `RoomChange`로 통일하고, `Broadcast`는 송신자 제외 정책으로 명확히 정의한다.
- 이 계약을 기준으로 구현하면 큰 패킷 전송에서 `RIO / IOCP` 성능 차이를 기능 노이즈 없이 비교하기 쉬워진다.
