# Plans 상태판

## 1. 상태 기준
- `완료`
  - 1차 구현과 기본 검증까지 끝난 묶음
- `진행 중`
  - 구현이나 검증이 아직 계속되고 있는 묶음
- `추가 확인 필요`
  - 구현은 들어갔지만 장시간 검증, 성능 재확인, 후속 판단이 남아 있는 묶음

## 2. 작업 상태
| 번호 | 작업 묶음 | 상태 | 비고 |
|---|---|---|---|
| `001` | Foundation | 추가 확인 필요 | 구조 계획은 있음. `Logging`, `Diagnostics`를 언제 실제 모듈로 올릴지 판단 필요 |
| `002` | Legacy MhLib 조사/정리 | 완료 | 레거시 참고 기준 정리 완료 |
| `003` | NetworkLib Crypto / Packet Header | 완료 | framing, header, cipher 방향 정리 및 반영 완료 |
| `004` | NetworkLib Session | 완료 | 세션/송신 기본 구조 정리 완료 |
| `005` | NetworkLib Packet View | 완료 | zero-copy packet view 기반 정리 완료 |
| `006` | Packet Schema Tooling | 완료 | `PacketGenerator`, generated packet/handler/router, `Login + Echo + Chat` 샘플까지 완료 |
| `007` | NetworkLib Performance | 추가 확인 필요 | 최적화는 적용됐지만 장시간/고부하 검증과 최종 성능 결론 재확인 필요 |
| `008` | ContentsRuntime | 진행 중 | 구조, 계측, enqueue 경량화, lock-free inbox 프로토타입 진행. 디렉터리 세분화와 `IContentBridge` 확장 반영, 안정성 검증 계속 필요 |

## 3. 현재 우선순위
1. `008_contents-runtime`
   - lock-free packet inbox 안정성 검증
   - race injection 기반 검증
   - 장시간 soak 결과 정리
2. `007_networklib-performance`
   - 장시간 고부하 벤치마크 재확인
   - 필요 시 추가 병목 분석
3. `001_foundation`
   - 공용 모듈 분리 시점과 범위 판단

## 4. 나중에 다시 확인할 항목
- `007_networklib-performance`
  - page pool 장시간 비교
  - 고부하 시나리오에서 send/recv copy 감소 효과 재검증
  - 서버 OS 환경에서의 재측정
- `008_contents-runtime`
  - lock-free inbox 2시간/8시간 검증
  - race injection on/off A/B 결과 정리
  - `Lobby -> Room` 같은 실제 전이 모델 추가 검증
- `001_foundation`
  - `Foundation/Logging`, `Foundation/Diagnostics` 구조 승격 시점 판단

## 5. 갱신 규칙
- `plans` 문서를 추가하거나 작업 상태가 바뀌면 이 상태판을 반드시 같이 갱신한다.
- `완료 -> 추가 확인 필요`처럼 상태가 되돌아가면 그 이유를 비고에 적는다.
