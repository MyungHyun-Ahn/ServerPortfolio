# Plans 상태판

## 1. 상태 기준
- `완료`
  - 1차 구현과 기본 검증까지 끝난 작업
- `진행 중`
  - 구현 또는 성능/안정성 검증이 계속 진행 중인 작업
- `추가 확인 필요`
  - 방향은 맞지만 후속 실험이나 정책 결정이 더 필요한 작업

## 2. 작업 상태
| 번호 | 작업 묶음 | 상태 | 비고 |
|---|---|---|---|
| `001` | Foundation | 진행 중 | `Diagnostics` RTT 계측, `Ids` allocator, `Config` YAML 로더와 `ConfigGenerator`까지 반영 완료. 이후 `Logging / Diagnostics / Config` 경계 정리와 reserve bit 후속 정책이 남아 있다. |
| `002` | Legacy MhLib 조사/정리 | 완료 | 과거 구조 비교와 참고 사항 정리 완료. |
| `003` | NetworkLib Crypto / Packet Header | 완료 | cipher, framing, content header 기반 정리 완료. |
| `004` | NetworkLib Session | 완료 | 세션 생명주기와 참조 관리 구조 정리 완료. |
| `005` | NetworkLib Packet View | 완료 | `string_view`, `bytes_view`, borrowed view guard 반영 완료. |
| `006` | Packet Schema Tooling | 완료 | `PacketGenerator`, generated packet/handler/router 반영 완료. |
| `007` | NetworkLib Performance | 진행 중 | `IOCP + RIO` 병행 지원 구조 분리, pure `RIO` baseline, `Rio Direct / Rio OwnerThread / Iocp` 2시간 A/B 비교, 기본 `SendPacket` 경로 교체, `IOCP AcceptEx` 전환과 재접속 stress 검증까지 완료. 현재 결론은 `Rio` 기본 send 정책은 `Direct` 유지가 적절하고, `IOCP`는 `AcceptEx + accept context slot pool` 기준으로 안정화되었다는 것이다. 다음 단계는 `RIO` 후속 최적화, broadcast fan-out, `IOCP AcceptEx` 성능 비교, 필요 시 socket reuse 실험 옵션 재검토다. |
| `008` | ContentsRuntime | 완료 | lock-free inbox 안정성 검증, 로비/룸 멀티 인스턴스, send lost-wakeup 수정, 무timeout 6시간 RTT 검증, `contentInstanceId` allocator 적용까지 완료. 이후 콘텐츠 확장은 별도 후속 작업으로 본다. |

## 3. 현재 우선순위
1. `007_networklib-performance`
   - `RIO` 후속 최적화와 `IOCP / RIO` 비교 결과 정리
   - `OwnerThread` 경로 최적화 가치 판단
   - 기본 `SendPacket` 경로 교체 이후 broadcast fan-out과 send copy 감소 구조 진행
   - `IOCP AcceptEx` 전환 후 accept path 성능 비교
   - 필요 시 `accepted socket reuse` 실험 옵션 재검토
2. `001_foundation`
   - `Logging / Diagnostics / Config` 경계 문서화
   - `contentInstanceId reserve` 비트 후속 정책 정리
3. 후속 확장 항목
   - 실제 새 콘텐츠 추가 시 `ContentsRuntime` 확장 재개
   - 멀티 콘텐츠 운영 정책과 인스턴스 배치 정책 구체화

## 4. 추후 다시 확인할 항목
- `007_networklib-performance`
  - page pool 장시간 비교
  - 큰 payload 조건에서 copy 감소 효과 확인
  - `OwnerThread` inbox 최적화 여부 판단
  - `RIO` buffer 등록/해제 비용 최적화
  - broadcast packet fan-out과 send copy 감소 구조 검증
  - `IOCP AcceptEx` 전환 후 accept path 성능 비교
  - `accepted socket reuse` 실험 옵션 재검토
- `008_contents-runtime`
  - 새 콘텐츠 타입 추가와 멀티 콘텐츠 확장
  - 정상 실패/비정상 실패 로그 정책 일반화
- `001_foundation`
  - 서버 측 RTT/진단 재사용 범위 정리
  - `contentInstanceId reserve` 비트를 분산 서버 `serverId`로 전환할 시점과 규칙 정리
