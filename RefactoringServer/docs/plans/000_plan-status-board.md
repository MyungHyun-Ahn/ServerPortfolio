# Plan Status Board

## 1. 상태 기준
- `Completed`
  - 구현과 기본 검증이 끝난 상태
- `In Progress`
  - 구현 또는 후속 검증이 진행 중인 상태
- `Needs Follow-up`
  - 방향은 정리됐지만 본 작업은 아직 시작하지 않은 상태

## 2. 작업 현황
| ID | 영역 | 상태 | 메모 |
| --- | --- | --- | --- |
| `001` | Foundation | In Progress | `Logging`, `Diagnostics`, `Config` 경계와 reserve-bit 규칙 정리 계속 진행 중 |
| `002` | Legacy MhLib Review | Completed | 과거 구조 비교와 참고 문서 정리 완료 |
| `003` | NetworkLib Crypto / Packet Header | Completed | cipher, framing, content header 구조 정리 완료 |
| `004` | NetworkLib Session | Completed | 세션 생명주기와 소유권 모델 정리 완료 |
| `005` | NetworkLib Packet View | Completed | borrowed view, `string_view`, `bytes_view` 규칙 정리 완료 |
| `006` | Packet Schema Tooling | Completed | `PacketGenerator`, generated packet/router 흐름 정리 완료 |
| `007` | NetworkLib Performance | In Progress | `RIO Registered Buffer Pool` 전환과 `FSendSegmentPool` lock-free TLS free-list 적용 완료. 최신 10분 RTT 비교에서 `Rio Direct` 우세 확인. 다음 과제는 `RIO send hot path` 오버헤드 감소 |
| `008` | ContentsRuntime | Completed | `content-owned mailbox + worker executor` 구조와 `mailbox owner transfer` 기반 `delegate / work stealing` 구현 및 `2026-04-06` 기준 인위적 부하 `3분`, `10분` 검증 완료 |
| `009` | WorldServer | Needs Follow-up | cell 기반 월드와 task graph는 별도 우선순위 과제로 분리됨 |

## 3. 현재 우선순위
1. `007_networklib-performance`
   - `RIO send hot path` 오버헤드 감소
   - `SSendRequestContext` 풀링, `RIOSend` batching, submit lock 정리
2. `001_foundation`
   - `Logging / Diagnostics / Config` 경계 정리 마무리
3. `009_worldserver`
   - 실제 월드 구조 설계 재개

## 4. ContentsRuntime 결론
- `content instance = dedicated thread` 구조는 제거했다.
- 현재 기준 구조는 `content-owned mailbox + worker executor`다.
- `delegate`와 `work stealing`은 별도 queue replay가 아니라 `mailbox owner transfer`로 구현했다.
- `move`와 `owner transfer` 충돌은 `pending move content transfer 금지` 가드로 막고 있다.
- 검증 기준:
  - `250 sessions`
  - `connectsPerSecond=10`
  - `interval=0`
  - `room-change=90%`
  - `Room 77 OnFrame Sleep 15ms`
  - `3분`, `10분` 성공

## 5. NetworkLib Performance 결론
- `RIO Registered Buffer Pool` 전환은 구현과 기본 검증을 완료했다.
- `FSendSegmentPool`은 `TLS local free-list + lock-free shared stack` 구조까지 반영됐다.
- 최신 1시간 4모드 고부하 비교는 `2026-04-06` 기준으로 다시 수행했다.
- 최신 10분 RTT 비교는 `2026-04-07` 기준으로 다시 수행했고, `Rio Direct > Rio OwnerThread > IOCP Default` 순으로 나왔다.
- 최신 총 응답 수 기준 순위:
  - `IocpSendBufDefault`
  - `IocpSendBuf0`
  - `Rio Direct`
  - `Rio OwnerThread`
- 현재 다음 병목은 `RIO send hot path` 오버헤드다.
