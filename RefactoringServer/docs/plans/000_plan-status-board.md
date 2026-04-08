# 계획 상태 보드

## 1. 상태 기준
- `완료`
  구현과 기본 검증이 끝난 상태
- `진행 중`
  구현은 시작됐고 후속 정리나 확장이 남아 있는 상태
- `후속 필요`
  방향은 정리됐지만 본격 구현은 아직 시작하지 않았거나 다음 단계로 미뤄둔 상태

## 2. 작업 현황
| ID | 영역 | 상태 | 메모 |
| --- | --- | --- | --- |
| `001` | Foundation | 진행 중 | `Logging`, `Diagnostics`, `Config` 경계와 공용 기반 규칙을 계속 정리 중 |
| `002` | Legacy MhLib Review | 완료 | 레거시 구조 비교와 참고 문서 정리 완료 |
| `003` | NetworkLib Crypto / Packet Header | 완료 | cipher, framing, content header 구조 정리 완료 |
| `004` | NetworkLib Session | 완료 | 세션 생명주기와 소유권 모델 정리 완료 |
| `005` | NetworkLib Packet View | 완료 | borrowed view, `string_view`, `bytes_view` 규칙 정리 완료 |
| `006` | Packet Schema Tooling | 진행 중 | 기본 `PacketGenerator` 흐름은 정리 완료. 다음은 `broadcast` schema contract와 payload packet 확장 |
| `007` | NetworkLib Performance | 진행 중 | `RIO Registered Buffer Pool`, `FSendSegmentPool`, RTT 비교까지 진행. 다음은 `RIO send hot path` |
| `008` | ContentsRuntime | 진행 중 | `content-owned mailbox + worker executor`, `mailbox owner transfer` 기반 `delegate / work stealing` 구현 완료. 적용 시나리오는 `009_chatting_server`에서 이어서 진행 |
| `009` | ChattingServer | 진행 중 | `ChattingServer` packet/flow 계약, `ClientNetworkLib`, `ChattingDummyClient` 1차 구현 완료. 다음은 운영 경로 정리와 레거시 로그인 경로 정리 |
| `010` | WorldServer | 후속 필요 | cell 기반 월드와 task graph는 별도 미래 우선순위 과제로 분리 |
| `011` | BenchmarkRunner | 완료 | `PowerShell + YAML manifest` 기반 `BenchmarkRunner Core`, `ChattingScenario`, `scripts/bench` 실행 진입점과 artifact 규칙 정리 완료 |
| `012` | Login Platform / WinForms Prototype | 완료 | `Node.js LoginServer`, Docker Infra, `WinForms HTTP 로그인/회원가입`, `LoginAuth`, 중복 로그인 강제 교체까지 1차 범위 구현 및 수동 검증 완료 |
| `013` | Connector Library | 진행 중 | `Libraries/Connector`, `Redis ticket store`, `ChattingServer LoginAuth` 연동 완료. 다음은 `MySQL` 쪽 후속 정리와 운영 설정 보강 |

## 3. 현재 우선순위
1. `013_connector_library`
   - `Redis ticket consume` 경로 운영 검증
   - `MySQL` 연동 범위와 경계 정리
2. `009_chatting_server`
   - `ChattingServer` packet/flow 계약 유지
   - `legacy LoginRq`와 `LoginAuthRq` 병행 경로 정리
   - 수동 흐름과 더미 흐름 회귀 확인
3. `007_networklib-performance`
   - `RIO send hot path` 오버헤드 감소
   - `SSendRequestContext`, `RIOSend` batching, submit lock 범위 정리
4. `006_packet-schema-tooling`
   - `broadcast` packet schema / generator contract 추가

## 4. ContentsRuntime 메모
- `content instance = dedicated thread` 구조가 현재 기준은 아니다.
- 현재 구조는 `content-owned mailbox + worker executor`다.
- `delegate`, `work stealing`은 별도 replay queue가 아니라 `mailbox owner transfer`로 구현돼 있다.
- `move`와 owner transfer 충돌은 pending-move content-transfer 금지 규칙으로 막고 있다.

## 5. NetworkLib Performance 메모
- `RIO Registered Buffer Pool` 전환과 기본 검증은 끝났다.
- `FSendSegmentPool`에는 `TLS local free-list + lock-free shared stack` 구조가 반영돼 있다.
- 다음 주요 성능 과제는 계속 `RIO send hot path` 쪽이다.

## 6. Chatting / Login 메모
- `ChattingServer`는 fan-out, room fan-out, end-to-end workload 비교를 위한 샘플 서버다.
- `ChattingDummyClient`는 `Login -> RoomList -> RoomChange -> Chatting -> Broadcast` 상태 머신으로 동작한다.
- 사용자용 `C# WinForms ChattingClient`는 `LoginServer HTTP 로그인/회원가입 -> LoginAuthRq` 흐름까지 붙어 있다.
- 로그인 인증은 현재 `Node.js LoginServer + Redis chat ticket + MySQL AccountDB` 구조로 분리돼 있다.
- `LoginAuth` 경로에서는 `Redis chat ticket + loginVersion` 검증과 `userId -> sessionId` 기반 중복 로그인 강제 종료가 구현돼 있다.
- `Connector`는 외부 인증 연동을 위해 `NetworkLib`, `ContentsRuntime` 밖에 둔 별도 계층이다.
