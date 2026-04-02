# Performance Benchmark Plan

## First Benchmark Note
- Initial fixed-workload comparison for `page pool on/off` has already been measured.
- Current observed result:
  - `page pool on` was about `4% to 5%` better in `recv/send TPS` than `page pool off`.
- Next quantitative step:
  - run the same fixed-workload scenario `3 to 5` times
  - compare average, min, and max
  - record CPU usage and working set together

## TODO
- Long-run benchmark is still pending.
- Required follow-up:
  - `2 hour` comparison
  - `8 hour` overnight comparison
  - summary document with stable average metrics and anomaly notes

## 1. 목적
- 최적화 전후를 같은 시나리오로 반복 측정한다.
- 결과를 리뷰 문서에 정량 수치로 남긴다.

## 2. 비교 대상
- baseline:
  - TLS 풀 적용 이전 또는 page pool 적용 이전 기준
- candidate:
  - TLS 풀 적용 후
  - 버퍼 재사용/page pool 적용 후
  - 세션 경량화 적용 후

## 3. 공통 테스트 시나리오

### 3.1 smoke
- `sessions=1`
- `count=1`
- `Login -> Echo`
- 기능 회귀 확인

### 3.2 medium
- `sessions=32`
- `count=32`
- `intervalMs=100`
- `packetsPerSend=1`
- reconnect 없음

### 3.3 heavy
- `sessions=100`
- `count=32`
- `intervalMs=100`
- `packetsPerSend=4`
- reconnect 확률 5%

### 3.4 long-run
- `sessions=100`
- `holdSeconds=7200`
- 스트레스 로그 저장

## 4. 수집 항목
- 평균 `acceptTPS`
- 평균 `recvTPS`
- 평균 `sendTPS`
- 평균 `wsaRecvTPS`
- 평균 `wsaSendTPS`
- 평균 CPU 사용량
- TLS pool usage / capacity
- 필요 시 peak working set

## 5. 문서화 방식
- review 문서에 아래 형태로 남긴다.
  - test scenario
  - baseline
  - candidate
  - delta
  - 해석

## 6. 해석 원칙
- TPS가 올라도 CPU가 과도하게 오르면 별도 표시
- `wsaSendTPS` 감소와 `sendTPS` 유지가 같이 나오면 batching 개선으로 본다.
- reconnect 시나리오에서 accept/close 흔들림이 줄면 세션 경량화 효과로 본다.

## 7. 산출물
- 실행 로그
- review 문서
- 필요 시 stress script 갱신
