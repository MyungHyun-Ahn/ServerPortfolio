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
| `001` | Foundation | 진행 중 | `Diagnostics` 공용 RTT 계측과 `Ids` 공용 allocator는 반영 완료. 다음 단계로 `Foundation/Config`, YAML 기반 설정 로딩, `ConfigGenerator` 추가를 진행한다. |
| `002` | Legacy MhLib 조사/정리 | 완료 | 과거 구조 참고 기준 정리 완료 |
| `003` | NetworkLib Crypto / Packet Header | 완료 | cipher, framing, content header 기반 정리 완료 |
| `004` | NetworkLib Session | 완료 | 세션, 송수신, 수명주기 정리 완료 |
| `005` | NetworkLib Packet View | 완료 | `string_view`, `bytes_view`, borrowed view guard 반영 완료 |
| `006` | Packet Schema Tooling | 완료 | `PacketGenerator`, generated packet/handler/router 반영 완료 |
| `007` | NetworkLib Performance | 추가 확인 필요 | 서버 OS 기준 장시간 성능 검증과 추가 벤치마크가 남아 있다. |
| `008` | ContentsRuntime | 완료 | lock-free inbox 안정성 검증, 로비/룸 멀티 인스턴스 흐름, send lost-wakeup 수정, 무timeout 6시간 RTT 검증, `uint64` 기반 `contentInstanceId` 인코딩과 allocator 적용까지 반영했다. 멀티 콘텐츠 확장과 운영 정책 정리는 후속 확장 항목으로 넘긴다. |

## 3. 현재 우선순위
1. `001_foundation`
   - `Foundation/Config` 기본 구조 추가
   - `EchoServer`, `EchoClient` YAML 설정 파일 체계 도입
   - `C# ConfigGenerator` 설계와 생성 파이프라인 추가
2. `007_networklib-performance`
   - 서버 OS 기준 장시간 성능 검증
   - 추가 병목 분석과 벤치마크
3. 후속 확장 항목
   - 새 콘텐츠 타입이 실제로 추가될 때 `ContentsRuntime` 확장 재개
   - 멀티 콘텐츠/멀티 인스턴스 배치 정책 구체화
   - 정상 실패/비정상 실패 운영 규칙 일반화

## 4. 나중에 다시 확인할 항목
- `007_networklib-performance`
  - page pool 장시간 비교
  - 큰 payload와 고부하 조건에서 send/recv copy 감소 효과 검증
  - 서버 OS 환경 비교
- `008_contents-runtime`
  - 새 콘텐츠 타입 추가 시 멀티 콘텐츠 확장 재개
  - 정상 실패/비정상 실패 로그 정책 재검증
  - 멀티 콘텐츠 타입과 멀티 인스턴스 배치 정책 구체화
- `001_foundation`
  - 서버 측 RTT/지연 계측 재사용 범위 정리
  - `Foundation/Logging`, `Foundation/Diagnostics`, `Foundation/Config` 경계 문서화
  - `contentInstanceId reserve` 비트를 분산 서버 `serverId`로 전환할 시점과 운영 규칙 정리
