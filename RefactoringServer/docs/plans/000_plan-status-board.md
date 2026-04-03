# Plans 상태판

## 1. 상태 기준
- `완료`
  - 1차 구현과 기본 검증까지 끝난 작업
- `진행 중`
  - 구현 또는 검증이 계속 진행 중인 작업
- `추가 확인 필요`
  - 구현은 들어갔지만 장시간 검증, 성능 검증, 후속 구조 확장이 남아 있는 작업

## 2. 작업 상태
| 번호 | 작업 묶음 | 상태 | 비고 |
|---|---|---|---|
| `001` | Foundation | 진행 중 | `Diagnostics` 공용 RTT 계측 승격 계획이 추가되었고, `Logging`/`Diagnostics` 경계 정리가 이어진다. |
| `002` | Legacy MhLib 조사/정리 | 완료 | 레거시 구조 참조 기준 정리 완료 |
| `003` | NetworkLib Crypto / Packet Header | 완료 | cipher, framing, content header 기반 정리 완료 |
| `004` | NetworkLib Session | 완료 | 세션, 송신 큐, 기본 수명주기 정리 완료 |
| `005` | NetworkLib Packet View | 완료 | `string_view`, `bytes_view`, borrowed view guard 반영 완료 |
| `006` | Packet Schema Tooling | 완료 | `PacketGenerator`, generated packet/handler/router 반영 완료 |
| `007` | NetworkLib Performance | 추가 확인 필요 | 서버 OS 기준 장시간 성능 검증과 추가 벤치마크가 남아 있다. |
| `008` | ContentsRuntime | 진행 중 | lock-free inbox 안정성 검증은 통과했고, 로비/룸 멀티 인스턴스 흐름이 구현되었다. 다음은 룸 흐름 확대 검증과 후속 문서화다. |

## 3. 현재 우선순위
1. `001_foundation`
   - `Diagnostics` 공용 RTT 계측 모듈 설계
   - `EchoClient` 분석용 계측을 공용 모듈로 승격
   - 향후 서버 계측 재사용 경계 정리
2. `008_contents-runtime`
   - `Login -> Lobby -> RoomList -> RoomEnter -> RoomChange -> RoomEcho` 흐름 정리
   - 멀티 콘텐츠/멀티 인스턴스 확장
   - 룸 관련 실패 코드, 재시도 정책, 장시간 검증 확대
3. `007_networklib-performance`
   - 서버 OS 기준 장시간 성능 검증
   - 추가 병목 분석 및 재측정

## 4. 나중에 다시 확인할 항목
- `007_networklib-performance`
  - page pool 장시간 비교
  - 더 큰 payload와 고부하 조건에서 send/recv copy 감소 효과 검증
  - 서버 OS 환경 비교
- `008_contents-runtime`
  - 로비/룸 흐름 장시간 soak
  - 정상 실패/비정상 실패 로그 정책 검증
  - 멀티 콘텐츠 타입과 멀티 인스턴스 배치 정책 구체화
- `001_foundation`
  - `Foundation/Logging`, `Foundation/Diagnostics` 구조 승격 판단
  - 공용 RTT collector / aggregator / sink 배치 기준 확정
