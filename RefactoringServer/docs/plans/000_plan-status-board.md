# Plan Status Board

## 1. 상태 기준
- `Completed`
  - 구현과 기본 검증이 끝난 상태
- `In Progress`
  - 구현 또는 후속 검증이 진행 중인 상태
- `Needs Follow-up`
  - 방향은 정리됐지만 본격 작업은 아직 시작하지 않은 상태

## 2. 작업 현황
| ID | 영역 | 상태 | 메모 |
| --- | --- | --- | --- |
| `001` | Foundation | In Progress | `Logging`, `Diagnostics`, `Config` 경계와 reserve-bit 규칙 정리 계속 진행 중 |
| `002` | Legacy MhLib Review | Completed | 레거시 프로젝트 구조 비교와 참고 문서 정리 완료 |
| `003` | NetworkLib Crypto / Packet Header | Completed | cipher, framing, content header 구조 정리 완료 |
| `004` | NetworkLib Session | Completed | 세션 생명주기와 소유권 모델 정리 완료 |
| `005` | NetworkLib Packet View | Completed | borrowed view, `string_view`, `bytes_view` 규칙 정리 완료 |
| `006` | Packet Schema Tooling | In Progress | 기본 `PacketGenerator` 흐름은 완료. 다음 후속은 `broadcast` schema contract와 payload packet 확장 정리 |
| `007` | NetworkLib Performance | In Progress | `RIO Registered Buffer Pool`, `FSendSegmentPool` 개선, 장시간 RTT 비교까지 진행. 다음 과제는 `RIO send hot path` 오버헤드 감소 |
| `008` | ContentsRuntime | In Progress | `content-owned mailbox + worker executor`, `mailbox owner transfer` 기반 `delegate / work stealing` 구현 완료. 응용 시나리오는 `009_chatting_server`로 분리 진행 |
| `009` | ChattingServer | In Progress | `ChattingServer` packet/flow 계약, `ClientNetworkLib`, `ChattingDummyClient` 1차 구현 완료. 다음은 반복 실험 자동화를 위한 `011_benchmark_runner` 연결 |
| `010` | WorldServer | Needs Follow-up | cell 기반 월드와 task graph는 별도 우선순위 과제로 분리 |
| `011` | BenchmarkRunner | Needs Follow-up | `PowerShell + YAML manifest` 기반 범용 벤치마크 실행기 추가 예정. 1차는 `ChattingScenario`부터 시작 |
| `012` | Login Platform / WinForms Prototype | In Progress | `ChattingClientWinForms` 1차 프로토타입 추가 완료. 현재는 permissive login + mock register 기준이며, 다음은 `Node.js LoginServer + Redis + MySQL` 연동 |

## 3. 현재 우선순위
1. `012_login-platform`
   - `C# WinForms` 로그인/회원가입/룸 선택/채팅 프로토타입 후속 안정화
   - 2차는 `Node.js + Redis + MySQL` 외부 인증 연동
2. `009_chatting_server`
   - `ChattingServer` packet/flow 계약 유지
   - `WinForms` 프로토타입이 붙을 수 있도록 packet/로그인 확장 경로 정리
   - 큰 패킷 / room fan-out 검증 경로는 기존 더미 클라이언트와 병행 유지
3. `011_benchmark_runner`
   - `PowerShell + YAML manifest` 기반 반복 실행기 추가
   - `ChattingServer / ChattingDummyClient` 시나리오를 범용 runner 구조에 연결
4. `007_networklib-performance`
   - `RIO send hot path` 오버헤드 감소
   - `SSendRequestContext`, `RIOSend` batching, submit lock 정리
5. `006_packet-schema-tooling`
   - `broadcast` packet schema / generator contract 추가

## 4. ContentsRuntime 메모
- `content instance = dedicated thread` 구조는 폐기됐다.
- 현재 기준 구조는 `content-owned mailbox + worker executor`다.
- `delegate`와 `work stealing`은 별도 queue replay가 아니라 `mailbox owner transfer`로 구현되어 있다.
- `move`와 `owner transfer` 충돌은 `pending move content transfer 금지` 가드로 막고 있다.
- 대표 검증 기준:
  - `250 sessions`
  - `connectsPerSecond=10`
  - `interval=0`
  - `room-change=90%`
  - `Room 77 OnFrame Sleep 15ms`
  - `3분`, `10분` 성공

## 5. NetworkLib Performance 메모
- `RIO Registered Buffer Pool` 전환은 구현과 기본 검증을 끝냈다.
- `FSendSegmentPool`에는 `TLS local free-list + lock-free shared stack` 구조까지 반영됐다.
- 최신 1시간 4모드 비교는 `2026-04-06` 기준으로 다시 수행됐다.
- 최신 10분 RTT 비교는 `2026-04-07` 기준으로 다시 수행됐고, `Rio Direct > Rio OwnerThread > IOCP Default` 순 우세가 관찰됐다.
- 현재 다음 병목 후보는 `RIO send hot path` 오버헤드다.

## 6. Chatting / BenchmarkRunner 메모
- `ChattingServer`는 큰 패킷과 room fan-out 비교용 샘플 서버다.
- `ChattingDummyClient`는 `Login -> RoomList -> RoomChange -> Chatting -> Broadcast 검증` 상태 머신으로 동작한다.
- 새 사용자용 `C# WinForms ChattingClient` 1차 프로토타입을 추가했고, 초기 로그인/회원가입은 mock 성공 기준으로 동작한다.
- 외부 인증은 이후 `Node.js LoginServer + Redis chat ticket + MySQL AccountDB` 구조로 분리한다.
- 다음 단계는 사람 손으로 모드를 바꿔 실행하는 대신, `PowerShell` 기반 범용 `BenchmarkRunner`로 반복 실험을 자동화하는 것이다.
- `BenchmarkRunner`는 1차로 `ChattingScenario`를 지원하고, 이후 `Echo`와 다른 더미 테스트에도 재사용 가능한 구조를 목표로 한다.
