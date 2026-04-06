# Plan Status Board

## 1. 상태 기준
- `Completed`
  - 구현과 핵심 검증이 끝난 상태
- `In Progress`
  - 구현 또는 검증이 계속 진행 중인 상태
- `Needs Follow-up`
  - 방향은 정리됐지만 지금 당장 진행하지 않는 상태

## 2. 작업 현황
| ID | 영역 | 상태 | 메모 |
|---|---|---|---|
| `001` | Foundation | In Progress | `Logging`, `Diagnostics`, `Config` 경계와 reserve-bit 규칙을 계속 정리 중이다. |
| `002` | Legacy MhLib Review | Completed | 레거시 구조 비교와 참고 문서 정리가 끝났다. |
| `003` | NetworkLib Crypto / Packet Header | Completed | cipher, framing, content header 기준 구조가 정리됐다. |
| `004` | NetworkLib Session | Completed | 세션 생명주기와 소유권 모델이 정리됐다. |
| `005` | NetworkLib Packet View | Completed | borrowed view, `string_view`, `bytes_view` 규칙이 정리됐다. |
| `006` | Packet Schema Tooling | Completed | `PacketGenerator`, generated packet/router 흐름이 정리됐다. |
| `007` | NetworkLib Performance | In Progress | `IOCP`, `RIO`, `SO_SNDBUF`, `AcceptEx` 비교는 정리됐다. 다음 비교는 최신 `ContentsRuntime` 위에서 다시 보는 것이 우선이다. |
| `008` | ContentsRuntime | Completed | `content-owned mailbox + worker executor` 구조, same-worker fast-path, deferred route commit, mailbox owner transfer 기반 `delegate / work stealing`까지 구현했고 `2026-04-06` 기준 3분/10분 고부하 안정성 검증을 통과했다. |
| `009` | WorldServer | Needs Follow-up | cell 기반 월드와 task graph는 장기 과제로 분리되어 있다. |

## 3. 현재 우선순위
1. `007_networklib-performance`
   - 최신 `ContentsRuntime` 기준으로 네트워크 백엔드 비교를 다시 본다.
   - `Rio Direct`, `Rio OwnerThread`, `Iocp` 값을 mailbox runtime 위에서 다시 측정한다.
2. `001_foundation`
   - `Logging / Diagnostics / Config` 경계를 계속 정리한다.
   - reserve-bit 정책과 identifier 문서를 마무리한다.
3. `009_worldserver`
   - 실제 월드 요구가 생길 때만 진행한다.

## 4. ContentsRuntime 결론
- `content instance = dedicated thread` 구조는 제거됐다.
- 현재 기준 구조는 `content-owned mailbox + worker executor`다.
- `delegate`와 `work stealing`은 별도 queue replay 시스템이 아니라 `mailbox owner transfer`로 구현됐다.
- `move`와 `owner transfer` 충돌은 `pending move content transfer 금지` 가드로 막았다.
- 검증 기준:
  - `250 sessions`
  - `connectsPerSecond=10`
  - `interval=0`
  - `room-change=90%`
  - `Room 77 OnFrame Sleep 15ms`
  - `3분`, `10분` 런 성공

## 5. 후속 메모
- `ContentsRuntime`는 현재 기준으로 신뢰 가능한 기본 구조로 본다.
- 추가 작업은 구조 교체가 아니라 정책 튜닝, 관측 지표 보강, 장시간 soak 검증 쪽이다.
