# 3분 성능 벤치마크 계획

## 1. 목적
- 짧은 스모크보다 긴 `3분` 구간에서 직렬화 경로 최적화의 평균 효과를 측정한다.
- TPS뿐 아니라 CPU, working set까지 같이 수집한다.

## 2. 시나리오
- `sessions=100`
- `count=32`
- `payloadSize=128`
- `holdSeconds=180`
- `intervalMs=100`
- `packetsPerSend=4`
- `reconnectProbabilityPercent=5`

## 3. 비교 대상
### 3-1. baseline
- 현재 커밋 기준 최적화 적용 전 상태

### 3-2. candidate
- 직렬화 경로 bulk 처리
- generated packet estimated size reserve 적용

## 4. 수집 항목
- 평균 `acceptTPS`
- 평균 `recvTPS`
- 평균 `sendTPS`
- 평균 `recvBps`
- 평균 `sendBps`
- 평균 `wsaRecvTPS`
- 평균 `wsaSendTPS`
- 평균 `cpuPercent`
- 평균 `workingSetMB`
- 최대 `peakWorkingSetMB`

## 5. 문서화
- 리뷰 문서에 `baseline / candidate / delta` 형식으로 추가
- 로그 파일 경로를 같이 남긴다.
