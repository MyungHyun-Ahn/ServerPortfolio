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
| `002` | Legacy MhLib Review | Completed | 과거 구조 비교와 참고 문서 정리 완료 |
| `003` | NetworkLib Crypto / Packet Header | Completed | cipher, framing, content header 구조 정리 완료 |
| `004` | NetworkLib Session | Completed | 세션 생명주기와 소유권 모델 정리 완료 |
| `005` | NetworkLib Packet View | Completed | borrowed view, `string_view`, `bytes_view` 규칙 정리 완료 |
| `006` | Packet Schema Tooling | In Progress | 기본 `PacketGenerator` 흐름은 완료. 다음 후속은 `broadcast` schema contract, payload packet 확장, C# 출력 지원 정리 |
| `007` | NetworkLib Performance | In Progress | `RIO Registered Buffer Pool`, `FSendSegmentPool`, monitoring runtime까지 반영. 다음 과제는 `RIO send hot path` 오버헤드 감소와 cache ping-pong 검증 |
| `008` | ContentsRuntime | In Progress | `content-owned mailbox + worker executor`, `mailbox owner transfer` 기반 `delegate / work stealing` 구현 완료. 다음 적용 시나리오는 `009_chatting_server`와 연결 |
| `009` | ChattingServer | In Progress | `ChattingServer` packet/flow 계약, `ClientNetworkLib`, `ChattingDummyClient` 1차 구현 완료. 다음은 C# 소비자와 실제 UI 클라이언트 연결 |
| `010` | WorldServer | Needs Follow-up | cell 기반 월드와 task graph는 별도 우선순위 과제로 분리 |
| `011` | BenchmarkRunner | Needs Follow-up | `PowerShell + YAML manifest` 기반 범용 benchmark runner 추가 완료. 다음은 시나리오 확장과 요약 포맷 고도화 |
| `012` | C# Support | In Progress | `ClientNetworkLib.CSharp`를 WinForms와 Unity가 같이 쓰는 공용 계층으로 설계. `PacketGenerator` C# 출력과 parity 검증 경로가 선행 과제 |

## 3. 현재 우선순위
1. `007_networklib-performance`
   - `RIO send hot path` 오버헤드 감소
   - `SSendRequestContext`, `RIOSend` batching, submit lock 정리
   - cache ping-pong 계측 검증
2. `009_chatting_server`
   - `ChattingServer` packet/flow 계약 유지
   - `ClientNetworkLib`, `ChattingDummyClient` 후속 검증
   - 큰 패킷 / room fan-out 검증경로 다듬기
3. `012_csharp_support`
   - `ClientNetworkLib.CSharp` 설계 확정
   - `PacketGenerator` C# 출력 범위 정의와 생성 구조 확정
   - WinForms / Unity 공용 API 원칙 정리
4. `011_benchmark_runner`
   - `PowerShell + YAML manifest` 기반 반복 실행기 보강
   - `ChattingServer / ChattingDummyClient` 시나리오를 더 일반화
5. `006_packet-schema-tooling`
   - `broadcast` packet schema / generator contract 추가
   - C# packet/codegen 출력 경로 설계

## 4. ContentsRuntime 메모
- `content instance = dedicated thread` 구조는 폐기됐다.
- 현재 기준 구조는 `content-owned mailbox + worker executor`다.
- `delegate`와 `work stealing`은 별도 queue replay가 아니라 `mailbox owner transfer`로 구현되어 있다.
- `move`와 `owner transfer` 충돌은 `pending move content transfer 금지` 정책으로 막고 있다.
- 대표 검증 기준:
  - `250 sessions`
  - `connectsPerSecond=10`
  - `interval=0`
  - `room-change=90%`
  - `Room 77 OnFrame Sleep 15ms`
  - `3분`, `10분` loaded run 성공

## 5. NetworkLib Performance 메모
- `RIO Registered Buffer Pool` 전환과 구현 검증은 끝났다.
- `FSendSegmentPool`은 `TLS local free-list + lock-free shared stack` 구조까지 반영됐다.
- 최신 Windows Server Echo 2시간 4모드와 Chatting 1시간 4모드 결과가 리뷰 문서로 정리돼 있다.
- 다음 핵심 병목 후보는 `RIO Direct send hot path`와 cross-thread send ring touch다.

## 6. Chatting / BenchmarkRunner 메모
- `ChattingServer`는 packet flow와 room fan-out 비교를 위한 대표 콘텐츠 서버다.
- `ChattingDummyClient`는 `Login -> RoomList -> RoomChange -> Chatting -> Broadcast 검증` 상태 머신으로 동작한다.
- benchmark는 `PowerShell + YAML` 기반 runner로 반복 실행할 수 있다.
- 다음 후속은 `ClientNetworkLib.CSharp`와 C# packet/codegen 지원을 추가해서 WinForms와 Unity 소비자 경로를 여는 것이다.
