# Plans 상태판

## 1. 보는 기준
- `완료`
  - 설계와 1차 구현, 기본 검증까지 끝난 묶음
- `진행 중`
  - 현재 확장/안정화/검증이 계속 진행 중인 묶음
- `추가 확인 필요`
  - 구현은 들어갔지만 장시간 검증이나 후속 판단이 남은 묶음

## 2. 상태판

| 번호 | 작업 묶음 | 상태 | 비고 |
|---|---|---|---|
| `001` | Foundation | 추가 확인 필요 | 구조 계획은 있음. `CrashDump`, `Logger`는 후속 구현/정착 필요 |
| `002` | Legacy MhLib 재사용 검토 | 완료 | 레거시 참고 기준 정리 완료 |
| `003` | NetworkLib Crypto / Packet Header | 완료 | 패킷 암호화, framing, header 방향 정리 및 적용 |
| `004` | NetworkLib Session | 완료 | 세션/송수신 기본 구조 정착 |
| `005` | NetworkLib Packet View | 완료 | `packet view`, zero-copy 기반 정리 완료 |
| `006` | Packet Schema Tooling | 완료 | `PacketGenerator`, generated packet/handler/router, `Login + Echo + Chat` 샘플 정착 |
| `007` | NetworkLib Performance | 추가 확인 필요 | 여러 최적화는 적용됨. 장시간/고부하 검증과 일부 성능 결론은 재확인 필요 |
| `008` | ContentsRuntime | 진행 중 | 구조, 계측, lock-free inbox 프로토타입, 전이 규칙까지 들어감. 장시간 안정성 검증 계속 필요 |

## 3. 현재 우선순위
1. `008_contents-runtime`
   - lock-free packet inbox 안정성 검증
   - race injection 기반 검증
   - 장시간 soak 결과 정리
2. `007_networklib-performance`
   - 장시간/고부하 벤치마크 재확인
   - 필요 시 추가 병목 분석
3. `001_foundation`
   - 공용 모듈 실제 정착 여부 재검토

## 4. 나중에 다시 봐야 할 항목
- `007_networklib-performance`
  - page pool 장시간 비교
  - 고부하 시나리오에서 send/recv copy 감소 효과 재확인
  - 데스크탑 환경이 아닌 더 신뢰 가능한 환경에서 재측정
- `008_contents-runtime`
  - lock-free inbox 2시간/8시간 검증
  - race injection on/off A/B 결과 정리
  - `Lobby -> Room` 같은 실제 전이 모델 추가 후 재검증
- `001_foundation`
  - `Foundation/Logging`, `Foundation/Diagnostics` 구조 정착 시점 재판단

## 5. 갱신 규칙
- 새 `plans` 문서를 추가하거나 작업 상태가 바뀌면 이 상태판을 반드시 함께 갱신한다.
- `완료 -> 추가 확인 필요`처럼 상태가 되돌아갈 수 있으면 그 이유를 비고에 짧게 남긴다.
