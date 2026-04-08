# 계획 상태 보드

## 1. 상태 기준
- `완료`
  구현과 기본 검증이 끝난 상태
- `진행 중`
  구현 또는 후속 검증이 실제로 진행 중인 상태
- `후속 필요`
  방향은 정리됐지만 본격 구현은 아직 시작하지 않은 상태

## 2. 작업 현황
| ID | 영역 | 상태 | 메모 |
| --- | --- | --- | --- |
| `001` | Foundation | 진행 중 | `Logging`, `Diagnostics`, `Config` 경계와 공용 기반 규칙을 계속 정리 중 |
| `002` | Legacy MhLib Review | 완료 | 레거시 구조 비교와 참고 문서 정리 완료 |
| `003` | NetworkLib Crypto / Packet Header | 완료 | cipher, framing, content header 구조 정리 완료 |
| `004` | NetworkLib Session | 완료 | 세션 생명주기와 소유권 모델 정리 완료 |
| `005` | NetworkLib Packet View | 완료 | borrowed view, `string_view`, `bytes_view` 규칙 정리 완료 |
| `006` | Packet Schema Tooling | 진행 중 | 기본 `PacketGenerator` 흐름은 완료. 다음은 `broadcast` schema contract와 payload packet 확장 정리 |
| `007` | NetworkLib Performance | 진행 중 | `RIO Registered Buffer Pool`, `FSendSegmentPool`, 장시간 RTT 비교 진행 중. 다음 병목은 `RIO send hot path` |
| `008` | ContentsRuntime | 진행 중 | `content-owned mailbox + worker executor`, `mailbox owner transfer` 기반 `delegate / work stealing` 구현 완료. 응용 시나리오는 `009_chatting_server`로 분리 진행 |
| `009` | ChattingServer | 진행 중 | `ChattingServer` packet/flow 계약, `ClientNetworkLib`, `ChattingDummyClient` 1차 구현 완료. 다음은 `011_benchmark_runner` 연결 |
| `010` | WorldServer | 후속 필요 | cell 기반 월드와 task graph는 별도 미래 우선순위 과제로 분리 |
| `011` | BenchmarkRunner | 후속 필요 | `PowerShell + YAML manifest` 기반 범용 실행기 추가 예정. 1차는 `ChattingScenario`부터 시작 |
| `012` | Login Platform / WinForms Prototype | 진행 중 | `ChattingClientWinForms` 1차 프로토타입 추가 완료. 현재는 permissive login + mock register 기준이며, 다음은 `Node.js LoginServer + Redis + MySQL` 연동 |
| `013` | Connector Library | 후속 필요 | `Libraries/Connector` 경계와 `Redis/MySQL` 정리 방향 문서화 완료. 다음은 `Connector.vcxproj` 스캐폴드와 `Redis ticket store` 인터페이스 추가 |

## 3. 현재 우선순위
1. `012_login-platform`
   - `C# WinForms` 로그인, 회원가입, 룸 선택, 채팅 프로토타입 후속 안정화
   - 2차는 `Node.js + Redis + MySQL` 외부 인증 연동
2. `013_connector_library`
   - `Libraries/Connector` 프로젝트 스캐폴드
   - `IChatTicketStore`, `Null/InMemory` 구현 추가
   - `ChattingServer` 주입 지점과 config 구조 정리
3. `009_chatting_server`
   - `ChattingServer` packet/flow 계약 유지
   - `ticket login` 확장 경로 정리
   - 더미 흐름과 실사용 흐름을 병행 유지
4. `011_benchmark_runner`
   - `PowerShell + YAML manifest` 기반 반복 실행기 추가
   - `ChattingServer / ChattingDummyClient` 시나리오를 범용 runner 구조에 연결
5. `007_networklib-performance`
   - `RIO send hot path` 오버헤드 감소
   - `SSendRequestContext`, `RIOSend` batching, submit lock 범위 정리
6. `006_packet-schema-tooling`
   - `broadcast` packet schema / generator contract 추가

## 4. ContentsRuntime 메모
- `content instance = dedicated thread` 구조는 현재 기준이 아니다.
- 현재 기준 구조는 `content-owned mailbox + worker executor`다.
- `delegate`, `work stealing`은 별도 replay queue가 아니라 `mailbox owner transfer`로 구현돼 있다.
- `move`와 owner transfer 충돌은 pending-move content-transfer 금지 규칙으로 막고 있다.

## 5. NetworkLib Performance 메모
- `RIO Registered Buffer Pool` 전환과 기본 검증은 끝났다.
- `FSendSegmentPool`에는 `TLS local free-list + lock-free shared stack` 구조가 반영돼 있다.
- 다음 의미 있는 성능 과제는 계속 `RIO send hot path` 쪽이다.

## 6. Chatting / Login 메모
- `ChattingServer`는 패킷, room fan-out, end-to-end workload 비교용 샘플 서버다.
- `ChattingDummyClient`는 `Login -> RoomList -> RoomChange -> Chatting -> Broadcast` 상태 머신으로 동작한다.
- 사용자용 `C# WinForms ChattingClient` 1차 프로토타입이 추가돼 있다.
- 외부 인증은 이후 `Node.js LoginServer + Redis chat ticket + MySQL AccountDB` 구조로 분리한다.
- `Connector`는 이 외부 인증 연동을 위해 `NetworkLib`, `ContentsRuntime` 밖에 두는 별도 인프라 계층이다.
