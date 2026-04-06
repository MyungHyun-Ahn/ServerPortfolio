# Plan Status Board

## 1. 상태 기준
- `Completed`
  - 1차 구현과 기준 검증이 끝난 상태
- `In Progress`
  - 구현 또는 검증이 계속 진행 중인 상태
- `Needs Follow-up`
  - 방향은 정리됐지만 우선순위를 뒤로 둔 상태

## 2. 작업 축
| ID | 영역 | 상태 | 메모 |
|---|---|---|---|
| `001` | Foundation | In Progress | `Diagnostics`, `Ids`, YAML `Config` + `ConfigGenerator`는 정착했다. 남은 일은 `Logging / Diagnostics / Config` 경계 정리와 reserve-bit 후속 정책 정리다. |
| `002` | Legacy MhLib Review | Completed | 레거시 구조 비교와 참조 메모가 정리돼 있다. |
| `003` | NetworkLib Crypto / Packet Header | Completed | cipher, framing, content header 기준 구조가 정리돼 있다. |
| `004` | NetworkLib Session | Completed | 세션 생명주기와 소유권 모델이 정리돼 있다. |
| `005` | NetworkLib Packet View | Completed | `string_view`, `bytes_view`, borrowed-view guard 작업이 끝났다. |
| `006` | Packet Schema Tooling | Completed | `PacketGenerator`와 generated packet/handler/router 흐름이 잡혀 있다. |
| `007` | NetworkLib Performance | In Progress | `IOCP + RIO` 이중 백엔드, pure `RIO` baseline, `Rio Direct / Rio OwnerThread / Iocp` 비교, `SO_SNDBUF` 비교, `SendPacket` 경로 개편, `IOCP AcceptEx` 전환이 완료됐다. 현재 수치는 같은 머신에서 서버와 클라이언트를 함께 돌린 상대 비교값으로 해석한다. |
| `008` | ContentsRuntime | Completed | `content instance = dedicated thread` 구조를 제거하고 `content-owned mailbox + worker executor` 구조로 전환했다. `pending move packet buffer`, deferred route commit, same-worker move fast-path까지 반영했고, `250세션 / connectsPerSecond=10 / interval=0 / room-change=90% / hold=600s / Room 77 15ms sleep` 조건의 10분 안정성 런을 통과했다. 기존 `delegate/work stealing` 경로는 코드에서 제거했고, 필요 시 mailbox 기반 재설계를 전제로만 재검토한다. |
| `009` | WorldServer | Needs Follow-up | Cell 기반 월드 시뮬레이션과 task-graph 실행은 미래 과제로 분리돼 있다. 방향은 `ContentsRuntime = executor`, `WorldContent = task graph owner`이고 우선순위는 매우 낮다. |

## 3. 현재 우선순위
1. `007_networklib-performance`
   - 새 `ContentsRuntime` baseline 위에서 네트워크 벤치마크를 다시 비교한다.
   - `Rio Direct`, `Rio OwnerThread`, `Iocp` 상대 차이를 mailbox runtime 기준으로 다시 본다.
   - broadcast fan-out과 send-copy 추가 절감을 이어간다.
2. `001_foundation`
   - `Logging / Diagnostics / Config` 경계를 정리한다.
   - `contentInstanceId` reserve-bit 후속 정책을 마무리한다.
3. 낮은 우선순위 확장 축
   - `ContentsRuntime`의 추가 content 타입 확장은 실제 요구가 생길 때 진행한다.
   - `WorldServer` task-graph는 월드 요구가 생길 때만 착수한다.

## 4. 후속 백로그
- `007_networklib-performance`
  - page-pool 재측정
  - 대형 payload copy 절감 검증
  - `RIO` buffer registration / release 비용 조정
  - broadcast packet fan-out 검증
  - `IOCP` send-path copy reduction vs `SO_SNDBUF=0` trade-off 재검토
- `008_contents-runtime`
  - bootstrap / control path 우선순위 큐 필요 여부 검토
  - worker 배치 정책 고도화
  - mailbox 기반 추가 content 타입 확장
  - mailbox 구조 위에서만 future delegate / work stealing 재설계 검토
- `009_worldserver`
  - cell task graph 모델
  - one-shot task, phase, barrier 실행 모델
  - world-content 내부 scheduler 설계
- `001_foundation`
  - 서버 RTT / diagnostics 재사용 경계 정리
  - 분산 서버가 실제 범위가 될 경우 `serverId` 같은 reserve-bit 전환 정책 검토
