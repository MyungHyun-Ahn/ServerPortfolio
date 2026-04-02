# Instrumentation Plan

## 1. 목적
- 최적화를 감이 아니라 수치로 진행할 수 있게 한다.
- 전후 비교 결과를 문서화할 수 있게 기본 계측 지점을 고정한다.

## 2. 1차 계측 지표
- 서버:
  - `acceptTPS`
  - `recvTPS`
  - `sendTPS`
  - `wsaRecvTPS`
  - `wsaSendTPS`
  - `recvBytesPerSec`
  - `sendBytesPerSec`
- 세션:
  - active session count
  - send queue depth
  - max observed concurrent send I/O
- 메모리:
  - `FSession` TLS pool capacity / usage
  - `FSendBuffer` TLS pool capacity / usage
  - packet buffer pool capacity / usage

## 3. 2차 계측 지표
- packet serialize count
- packet serialize bytes
- framer build count
- framer extract count
- disconnect reason count
- reconnect count

## 4. 출력 위치
- 1차:
  - 서버 콘솔 1초 단위 출력
  - 서버 파일 로그 요약
- 2차:
  - stress test 종료 후 summary log
  - review 문서용 정리 표

## 5. 계측 원칙
- hot path에서 락을 새로 추가하지 않는다.
- `std::atomic<uint64_t>` 위주의 누적 카운터로 간다.
- 1초 delta는 출력 시점에만 계산한다.
- 상세 tracing은 기본 비활성화다.

## 6. 비교 기준
- 같은 설정으로 최소 2회 이상 측정
- 비교 항목:
  - 평균 `recvTPS`
  - 평균 `sendTPS`
  - 평균 `wsaSendTPS`
  - 평균 `wsaRecvTPS`
  - 평균 CPU 사용량
  - peak memory / pool usage

## 7. 성공 기준
- 문서에서 개선 전후를 같은 시나리오로 비교 가능
- 다음 최적화 우선순위를 수치로 설명 가능
