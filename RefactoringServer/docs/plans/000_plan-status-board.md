# 계획 상태 보드

## 1. 상태 기준
- `완료`
  구현과 기본 검증이 끝난 상태
- `진행 중`
  구현을 시작했고 후속 정리나 확장이 남아 있는 상태
- `후속 필요`
  방향은 잡혔지만 본격 구현은 다음 단계로 미뤄둔 상태

## 2. 작업 현황
| ID | 영역 | 상태 | 메모 |
| --- | --- | --- | --- |
| `001` | Foundation | 진행 중 | `Logging`, `Diagnostics`, `Config` 경계와 공용 규칙 정리 지속 |
| `002` | Legacy MhLib Review | 완료 | 과거 구조 비교와 참고 문서 정리 완료 |
| `003` | NetworkLib Crypto / Packet Header | 완료 | cipher, framing, content header 구조 정리 완료 |
| `004` | NetworkLib Session | 완료 | 세션 생명주기와 소유권 모델 정리 완료 |
| `005` | NetworkLib Packet View | 완료 | borrowed view, `string_view`, `bytes_view` 규칙 정리 완료 |
| `006` | Packet Schema Tooling | 진행 중 | 기본 `PacketGenerator` 흐름은 정리 완료. 다음은 `broadcast` contract, payload 확장, C# 출력 보강 |
| `007` | NetworkLib Performance | 진행 중 | `RIO Registered Buffer Pool`, `FSendSegmentPool`, monitoring/runtime, cache ping-pong 계측까지 반영 |
| `008` | ContentsRuntime | 진행 중 | `content-owned mailbox + worker executor`, `mailbox owner transfer`, `delegate / work stealing` 적용 완료 |
| `009` | ChattingServer | 진행 중 | `ChattingServer`, `ChattingDummyClient`, `ChattingClientWinForms`, `LoginAuth` 경로가 함께 진행 중 |
| `010` | WorldServer | 후속 필요 | cell 기반 월드와 task graph는 별도 미래 과제로 분리 |
| `011` | BenchmarkRunner | 완료 | `PowerShell + YAML manifest` 기반 범용 runner와 `scripts/bench` 체계 정리 완료 |
| `012_csharp_support` | C# Support | 진행 중 | `PacketGenerator` C# 출력과 `ClientNetworkLib.CSharp` 공용화 계획 진행 중 |
| `012_login-platform` | Login Platform | 완료 | `Node.js LoginServer`, Docker Infra, WinForms HTTP 인증, `LoginAuth`, 중복 로그인 교체까지 1차 완료 |
| `013_connector_library` | Connector Library | 진행 중 | `Libraries/Connector`, Redis ticket store, `ChattingServer LoginAuth` 연동 완료. 다음은 MySQL 쪽 경계 정리 |

## 3. 현재 우선순위
1. `013_connector_library`
   - Redis ticket consume 경로 운영 검증
   - MySQL 연동 범위와 책임 경계 정리
2. `009_chatting_server`
   - `LoginAuthRq`와 기존 `LoginRq` 병행 정책 정리
   - WinForms 사용자 흐름과 더미 benchmark 흐름 동시 유지
3. `007_networklib-performance`
   - `RIO Direct send hot path` 오버헤드 감소
   - cross-thread send ring touch와 cache ping-pong 검증 후속
4. `012_csharp_support`
   - `PacketGenerator` C# 출력 범위 확정
   - `ClientNetworkLib.CSharp` 공용 API 설계
5. `006_packet-schema-tooling`
   - C++/C# parity 검증과 generator contract 보강

## 4. ContentsRuntime 메모
- 현재 구조는 `content-owned mailbox + worker executor` 기준이다.
- `delegate`, `work stealing`은 별도 replay queue가 아니라 `mailbox owner transfer`로 구현되어 있다.
- `move`와 owner transfer 충돌은 pending move content transfer 금지 규칙으로 막고 있다.

## 5. NetworkLib Performance 메모
- `RIO Registered Buffer Pool`과 `FSendSegmentPool`은 반영되어 있다.
- `NetworkLib/Diagnostics` 아래 공용 monitoring runtime과 RIO TLS metrics runtime 분리가 완료됐다.
- 최근 핵심 검증 포인트는 `RIO Direct`의 cross-thread send ring touch와 cache ping-pong 영향이다.

## 6. Chatting / Login 메모
- `ChattingServer`는 fan-out, room 전환, end-to-end 인증 흐름을 함께 검증하는 샘플 서버다.
- `ChattingDummyClient`는 benchmark 전용이고, `ChattingClientWinForms`는 사용자 시연용이다.
- 로그인 플랫폼은 `Node.js LoginServer + Redis chat ticket + MySQL AccountDB`로 분리되어 있다.
- `LoginAuth` 경로에서는 Redis ticket과 `loginVersion` 검증, `userId -> sessionId` 기준 중복 로그인 강제 종료가 반영되어 있다.
- `Connector`는 이런 외부 인증/저장소 연동을 `NetworkLib`, `ContentsRuntime` 바깥에서 관리하기 위한 경계다.
