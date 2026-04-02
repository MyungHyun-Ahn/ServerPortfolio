# 성능 벤치마크 계획

## 1. 목적
- 최적화 전후를 같은 시나리오로 반복 측정한다.
- 결과를 리뷰 문서에 정량 수치로 남긴다.

## 2. 1차 측정 메모
- 초기 고정 작업량 비교에서 page pool `on/off`를 한 번 측정했다.
- 현재 관측 결과:
  - `page pool on`이 `recv/send TPS` 기준으로 약 `4% ~ 5%` 더 좋았다.
- 이 결과는 1차 기준선으로 보고, 반복 측정으로 확정해야 한다.

## 3. 비교 대상
### 3-1. baseline
- page pool 비활성화 기준

### 3-2. candidate
- page pool 활성화 기준
- 이후 필요하면 TLS 풀 적용 전후, 세션 경량화 전후도 같은 방식으로 추가 비교한다.

## 4. 공통 테스트 시나리오
### 4-1. smoke
- `sessions=1`
- `count=1`
- `Login -> Echo`
- 기능 무결성 확인용

### 4-2. medium
- `sessions=32`
- `count=32`
- `intervalMs=100`
- `packetsPerSend=1`
- reconnect 없음

### 4-3. heavy
- `sessions=100`
- `count=32`
- `intervalMs=100`
- `packetsPerSend=4`
- reconnect 확률 `5%`

### 4-4. fixed-workload
- `sessions=50`
- `count=64`
- `payloadSize=128`
- `packetsPerSend=4`
- `holdSeconds=0`
- page pool on/off 비교용 기준 시나리오

### 4-5. long-run
- `sessions=100`
- `holdSeconds=7200`
- 장시간 로그 수집용

## 5. 수집 항목
- 평균 `acceptTPS`
- 평균 `recvTPS`
- 평균 `sendTPS`
- 평균 `recvBps`
- 평균 `sendBps`
- 평균 `wsaRecvTPS`
- 평균 `wsaSendTPS`
- 평균 CPU 사용량
- pool usage / capacity
- 필요 시 peak working set

## 6. 문서화 방식
- 리뷰 문서에 아래 형식으로 추가한다.
  - 테스트 시나리오
  - baseline
  - candidate
  - delta
  - 해석

## 7. 해석 기준
- TPS가 오르고 CPU가 같거나 내려가면 긍정적 개선으로 본다.
- `wsaSendTPS` 감소와 `sendTPS` 유지가 함께 나오면 batching 또는 재사용 개선 가능성으로 본다.
- reconnect 시나리오에서 accept/close 흔들림이 줄면 세션 경량화 효과로 본다.

## 8. TODO
- 장시간 벤치마크는 아직 미완료다.
- 다음 후속 작업:
  - `2시간` 비교
  - `8시간` 야간 비교
  - 평균, 최소, 최대값 정리
  - CPU, working set, pool usage 안정성까지 포함한 요약 문서 작성
