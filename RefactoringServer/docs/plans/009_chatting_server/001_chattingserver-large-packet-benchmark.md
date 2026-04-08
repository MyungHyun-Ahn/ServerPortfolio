# ChattingServer Large Packet Benchmark Plan

## 1. 목적
- `RIO Direct`와 `IOCP`의 큰 패킷 처리 성능을 실제 서비스 형태에 가까운 시나리오로 검증할 수 있는 `ChatServer / ChatClient` 샘플을 만든다.
- 기존 `EchoServer`는 작은 request-response에 치우쳐 있으므로, `room 입장`, `room 변경`, `대용량 chat`, `room broadcast fan-out` 경로를 따로 검증할 수 있는 샘플이 필요하다.
- 특히 `RIO session send ring` 구조에서 큰 payload와 room broadcast가 실제로 어떤 병목을 만드는지 재현 가능한 환경을 확보한다.
- 1차 구현 기준 packet surface와 `송신자 제외 Broadcast` 정책은 [002_chattingserver-packet-and-flow-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\009_chatting_server\002_chattingserver-packet-and-flow-plan.md)를 우선 기준으로 삼는다.

## 2. 목표
- 최소 기능:
  - `Login`
  - `RoomList`
  - `RoomEnter`
  - `RoomChange`
  - `ChatSend`
  - `ChatBroadcast`
- 검증 목표:
  - 큰 단일 패킷 송수신
  - room 내 다수 세션 대상 broadcast fan-out
  - room 이동과 chat traffic이 동시에 있을 때의 안정성
  - backend별 throughput / RTT / CPU 비교

## 3. 배경
- 현재 스모크/고부하 시나리오는 대부분 `Echo` 중심이다.
- `Echo`는 `1:1 request-response` 관찰에는 좋지만, 다음 특성을 충분히 검증하지 못한다.
  - room fan-out
  - 다수 세션 동일 payload 전송
  - 수 KiB 단위 payload
  - room change와 send pressure의 동시 발생
- 앞으로 `RIO` broadcast 경로, 큰 payload 정책, session send ring의 headroom을 논할 때는 `Chat` 시나리오가 더 적합하다.

## 4. 범위
대상:
- 새 `ChatServer` 샘플 또는 `EchoServer`와 분리된 채팅 전용 실행 진입점
- 채팅용 클라이언트 시나리오
- `Login`, `Chat` 계열 packet schema
- room 목록 / 입장 / 이동 / 채팅 / broadcast 검증 로직

비범위:
- 영속 저장소
- 인증 토큰/계정 시스템
- 메시지 히스토리 저장
- moderation, profanity filtering, read receipt

## 5. 패킷 구성
### 5-1. 재사용
- `Login`은 기존 [Login.yaml](D:\Project\ServerPortfolio\RefactoringServer\Packet\Login\Login.yaml) 흐름을 재사용한다.
- `RoomList`, `RoomEnter`, `RoomChange`는 현재 [Chat.yaml](D:\Project\ServerPortfolio\RefactoringServer\Packet\Chat\Chat.yaml) 정의를 기반으로 유지 또는 확장한다.

### 5-2. 추가 필요 패킷
- `ChatSendRq`
  - 클라이언트가 방에 텍스트/바이너리 메시지를 보낸다.
- `ChatSendRp`
  - 선택 사항.
  - 서버 수락 여부, 메시지 번호, 검증용 timestamp를 줄 수 있다.
- `ChatBroadcast`
  - 서버가 room 내 참여자들에게 fan-out 하는 one-way packet
  - sender session/user, roomId, message sequence, payload, timestamp 포함

### 5-3. payload 정책
- 큰 패킷 검증이 목적이므로 payload는 `string`보다 `bytes` 또는 `vector<uint8>` 기준으로 설계하는 쪽이 낫다.
- 텍스트 검증이 필요하면 `text`와 `blob`을 분리한다.
- 예시:
  - `messageId : uint64`
  - `roomId : uint32`
  - `senderUserId : uint64`
  - `payload : bytes`
  - `sentTick : uint64`

## 6. 서버 시나리오
### 6-1. 기본 흐름
1. 클라이언트 연결
2. `LoginRq/Rp`
3. `RoomListRq/Rp`
4. 임의 room `RoomEnterRq/Rp`
5. 일정 확률로 `RoomChangeRq/Rp`
6. 나머지 시간은 `ChatSendRq`
7. 서버는 room 참여자 전체에 `ChatBroadcast` 전송

### 6-2. room 모델
- room 수는 config로 조절
- room capacity도 config로 조절
- room 내 참여자 수가 많을수록 broadcast fan-out pressure가 커지므로, small / medium / large room 분포를 나눌 수 있게 한다.

### 6-3. 검증 포인트
- 보낸 메시지의 `messageId`가 room 내 broadcast에서 유실되지 않는지
- 자기 자신에게도 broadcast가 오는지 여부를 정책으로 고정
- room change 직후 이전 room에서 더 이상 broadcast를 받지 않는지
- 큰 payload가 sequence 깨짐 없이 전달되는지

## 7. 클라이언트 시나리오
### 7-1. 기본 동작
- 각 세션은 로그인 후 room 목록을 받고 room에 입장한다.
- 입장 후 일정 주기로 `ChatSendRq`를 보낸다.
- room change 확률이 있으면 주기적으로 방을 바꾼다.
- 수신한 `ChatBroadcast`는 다음을 검증한다.
  - `roomId`
  - `messageId`
  - payload 길이
  - payload checksum 또는 prefix/suffix

### 7-2. 대용량 payload 프로필
- `1 KiB`
- `2 KiB`
- `4 KiB`
- `8 KiB 근접`
- 필요하면 mixed workload:
  - small 80%
  - medium 15%
  - large 5%

### 7-3. RTT 측정
- 최소 RTT 기준은 `ChatSendRq -> ChatBroadcast(self)` 또는 `ChatSendRq -> ChatSendRp` 중 하나로 정한다.
- 큰 패킷 fan-out이 목적이면 `ChatBroadcast` 도착 시간을 기준으로 잡는 쪽이 실제 체감과 가깝다.

## 8. ContentsRuntime 구성
### 8-1. 권장 구조
- `Auth`
- `Lobby`
- `ChatRoom`

### 8-2. 이유
- `RoomList`는 `Lobby`
- `RoomEnter/Change`는 `Lobby <-> ChatRoom`
- `ChatSend/Broadcast`는 `ChatRoom`
- 즉 기존 `Echo`에서 검증한 `room 이동`과 새 `broadcast send pressure`를 같은 샘플 안에서 다 볼 수 있다.

### 8-3. Broadcast 처리 위치
- room mailbox 안에서 room participant snapshot을 잡고 fan-out
- 큰 packet/broadcast 실험에서는 room 내부 loop가 병목이 될 수 있으므로, 추후 `broadcast batching`이나 `shared payload` 실험도 여기에 걸 수 있다.

## 9. 구현 순서
1. `Chat.yaml`에 `ChatSend`, `ChatBroadcast` 정의 추가
2. `PacketGenerator` 재생성
3. `ChatServer` contents 흐름 구현
   - login
   - room list
   - room enter
   - room change
   - chat send
   - room broadcast
4. `ChatClient` 검증 로직 구현
5. 작은 payload 스모크
6. 큰 payload 고부하
7. backend별 비교
   - `RIO Direct`
   - `IOCP Default`
   - 필요 시 `IOCP SendBuf0`

## 10. 설정 항목
- `ChatPayloadBytes`
- `ChatPayloadPattern`
- `ChatPacketsPerSend`
- `ChatBroadcastSelfEchoEnabled`
- `ChatRoomChangeProbabilityPercent`
- `ChatRoomCount`
- `ChatRoomCapacity`
- `ChatRttCsvPath`

## 11. 검증 계획
### 11-1. 기능
- 단일 세션 chat 송수신
- 다중 세션 동일 room broadcast
- room change 후 이전 room broadcast 차단
- 큰 payload checksum 검증

### 11-2. 성능
- 5분 고부하
- 30분 안정성
- 2시간 비교 런

### 11-3. 비교 지표
- total responses / broadcasts
- room broadcast TPS
- CPU%
- RTT avg / max / timeout
- send ring used bytes
- stall count

## 12. 기대 효과
- `Echo`로는 보기 어려운 큰 패킷 / room fan-out 병목을 재현할 수 있다.
- `RIO Direct`의 강점과 한계를 실제 broadcast workload에서 확인할 수 있다.
- 이후 `broadcast payload 공유`, `chat room batching`, `large packet policy` 같은 후속 최적화의 기준 샘플이 된다.
