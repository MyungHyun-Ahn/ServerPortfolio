# 네트워크 성능 최적화 리뷰

## 1. 목적
- `NetworkLib` 성능 최적화 작업의 적용 상태와 측정 결과를 한 문서에서 정리한다.
- 현재 단계에서 어떤 결과를 신뢰할 수 있고, 어떤 결과는 보류해야 하는지 분명히 남긴다.

## 2. 적용된 최적화
### 2-1. 메모리 재사용
- `FSession`에 TLS 풀을 적용했다.
- `FSendBuffer`에 TLS 풀을 적용했다.
- `FPacketWriter` 내부 버퍼가 `FPacketBuffer` 기반 재사용 경로를 타도록 정리했다.
- `FPacketBuffer`, `FSendBuffer`는 page 성격의 capacity 재사용이 가능하도록 정리했다.

### 2-2. 계측 추가
- 서버 통계에 아래 항목을 추가했다.
  - `acceptTPS`
  - `recvTPS`
  - `sendTPS`
  - `recvBps`
  - `sendBps`
  - `wsaSendTPS`
  - `wsaRecvTPS`
  - `cpuPercent`
  - `workingSetMB`
  - `peakWorkingSetMB`
  - `queuedSendBuffers`
  - `maxQueuedSendBuffers`
  - `sessionPool`
  - `sendBufferPool`
  - `packetBufferPool`

### 2-3. 세션/컨텍스트 경량화
- `EchoServer` 로그인 상태 관리를 `unordered_map + mutex`에서 세션 슬롯 기반 원자 배열로 변경했다.

### 2-4. 송신 프레이밍 복사 감소
- send 경로에서 `payloadBuffer -> framedBuffer`로 한 번 더 복사하는 경로를 줄였다.
- 현재는 `FSendBuffer`가 frame header 조각과 payload 조각을 send completion 시점까지 소유한다.

### 2-5. 패킷 직렬화 경로 최적화
- `FPacketWriter`
  - `vector<T>`에서 `T`가 scalar면 bulk `WriteBytes()` 경로를 사용한다.
  - `array<T, N>`에서 `T`가 scalar면 bulk `WriteBytes()` 경로를 사용한다.
- `FPacketReader`
  - `vector<T>`에서 `T`가 scalar면 `resize()` 후 bulk `ReadBytes()` 경로를 사용한다.
  - `array<T, N>`에서 `T`가 scalar면 bulk `ReadBytes()` 경로를 사용한다.

### 2-6. 패킷 생성 코드 최적화
- `IContentPacket`에 `GetEstimatedBodySize()`를 추가했다.
- generated packet이 각 필드 기준 예상 직렬화 크기를 계산해 override 하도록 만들었다.
- `SerializeContentBody()`, `SendContentPacket()`는 `GetEstimatedBodySize()`를 이용해 reserve 하도록 정리했다.

## 3. Page Pool 비교 결과
### 3-1. 측정 조건
- `sessions=50`
- `requestCount=64`
- `payloadSize=128`
- `packetsPerSend=4`
- `holdSeconds=0`
- `intervalMs=0`
- `sendChunkSize=7`
- `recvBufferSize=13`

### 3-2. 측정 결과
- `page pool on`
  - `AvgRecvTPS = 185.59`
  - `AvgSendTPS = 185.65`
  - `AvgRecvBps = 25530.76`
  - `AvgSendBps = 24131.88`
  - `AvgWsaRecvTPS = 3655.59`
- `page pool off`
  - `AvgRecvTPS = 177.78`
  - `AvgSendTPS = 177.78`
  - `AvgRecvBps = 24312.28`
  - `AvgSendBps = 23113.89`
  - `AvgWsaRecvTPS = 3481.44`

### 3-3. 해석
- 현재 고정 작업량 Echo 벤치마크 기준으로 `page pool on`이 `off`보다 대략 `4% ~ 5%` 정도 더 좋았다.
- 이 결과는 단기 비교 기준에서 참고할 만하다.

### 3-4. 근거 로그
- [echo_server_perf_20260402_105058.log](D:\Project\ServerPortfolio\RefactoringServer\Out\perf\echo_server_perf_20260402_105058.log)
- [echo_server_perf_20260402_105117.log](D:\Project\ServerPortfolio\RefactoringServer\Out\perf\echo_server_perf_20260402_105117.log)

## 4. BuildPacket 복사 감소 결과
### 4-1. 측정 결과
- 변경 전 평균
  - `AvgRecvTPS = 163.16`
  - `AvgSendTPS = 163.16`
  - `AvgRecvBps = 22402.87`
  - `AvgSendBps = 21202.63`
  - `AvgWsaRecvTPS = 3208.14`
- 변경 후 평균
  - `AvgRecvTPS = 163.16`
  - `AvgSendTPS = 163.16`
  - `AvgRecvBps = 22383.53`
  - `AvgSendBps = 21202.63`
  - `AvgWsaRecvTPS = 3205.27`

### 4-2. 해석
- 현재 Echo 고정 작업량 시나리오에서는 `BuildPacket` 복사 감소만으로 눈에 띄는 TPS 개선은 확인되지 않았다.

### 4-3. 근거 로그
- 변경 전
  - [echo_server_perf_20260402_113537.log](D:\Project\ServerPortfolio\RefactoringServer\Out\perf\echo_server_perf_20260402_113537.log)
  - [echo_server_perf_20260402_113559.log](D:\Project\ServerPortfolio\RefactoringServer\Out\perf\echo_server_perf_20260402_113559.log)
- 변경 후
  - [echo_server_perf_20260402_113809.log](D:\Project\ServerPortfolio\RefactoringServer\Out\perf\echo_server_perf_20260402_113809.log)
  - [echo_server_perf_20260402_113831.log](D:\Project\ServerPortfolio\RefactoringServer\Out\perf\echo_server_perf_20260402_113831.log)

## 5. CPU / 메모리 계측 확인
### 5-1. 추가한 항목
- `cpuPercent`
- `workingSetMB`
- `peakWorkingSetMB`

### 5-2. 스모크 확인 결과
- headless 서버 출력에서 위 세 항목이 실제로 1초 통계에 포함되는 것을 확인했다.

### 5-3. 근거 로그
- [cpu_mem_smoke_server.log](D:\Project\ServerPortfolio\RefactoringServer\Out\cpu_mem_smoke_server.log)
- [cpu_mem_smoke_client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\cpu_mem_smoke_client.log)

## 6. 3분 성능 비교 결과
### 6-1. 측정 조건
- `sessions=100`
- `requestCount=32`
- `payloadSize=128`
- `holdSeconds=180`
- `intervalMs=100`
- `packetsPerSend=4`
- `reconnectProbabilityPercent=5`
- `sendChunkSize=7`
- `recvBufferSize=13`

### 6-2. baseline
- 기준 커밋: `78ce5fd`
- [echo_server_perf_20260402_121225.log](D:\Project\ServerPortfolio_baseline_seropt\RefactoringServer\Out\perf\echo_server_perf_20260402_121225.log)
- 평균 수치
  - `AvgAcceptTPS = 1.07`
  - `AvgRecvTPS = 390.32`
  - `AvgSendTPS = 389.19`
  - `AvgRecvBps = 52234.55`
  - `AvgSendBps = 49899.80`
  - `AvgWsaRecvTPS = 7482.65`
  - `AvgWsaSendTPS = 389.19`
  - `AvgCpuPercent = 0.46`
  - `AvgWorkingSetMB = 20.66`
  - `PeakWorkingSetMB = 24.35`

### 6-3. candidate
- 현재 후보 로그
  - [echo_server_perf_20260402_121608.log](D:\Project\ServerPortfolio\RefactoringServer\Out\perf\echo_server_perf_20260402_121608.log)
- 평균 수치
  - `AvgAcceptTPS = 0.96`
  - `AvgRecvTPS = 354.84`
  - `AvgSendTPS = 354.84`
  - `AvgRecvBps = 47591.40`
  - `AvgSendBps = 45473.12`
  - `AvgWsaRecvTPS = 6817.69`
  - `AvgWsaSendTPS = 354.84`
  - `AvgCpuPercent = 0.35`
  - `AvgWorkingSetMB = 18.73`
  - `PeakWorkingSetMB = 21.75`

### 6-4. 결론
- 이번 3분 비교 결과는 **신뢰 가능한 성능 결론으로 사용하지 않는다**.
- 이유는 아래와 같다.
  - 단일 실행 1회 비교뿐이다.
  - 데스크탑 Windows 환경이라 백그라운드 스케줄링 영향이 크다.
  - CPU 감소 수치가 실제 최적화 효과인지, 측정 시점 노이즈인지 분리할 수 없다.
  - TPS와 CPU가 서로 다른 방향으로 움직였지만, 반복 측정 없이 원인을 확정할 수 없다.
- 따라서 이번 결과는 **참고 로그 확보** 수준으로만 남기고, `candidate가 더 좋다` 또는 `더 나쁘다`는 정량 결론은 내리지 않는다.

## 7. 검증 근거
- 빌드
  - `RefactoringServer.sln` x64 Debug
- 테스트
  - [LockFreeTests.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\LockFreeTests.exe)
- 추가 회귀
  - `Login -> Echo` 스모크 실행 통과

## 8. TODO
- 같은 3분 시나리오를 3회 이상 반복해서 평균과 분산을 다시 계산할 필요가 있다.
- 더 높은 부하 조건에서 다시 측정할 필요가 있다.
  - 더 많은 세션 수
  - 더 큰 payload
  - 더 높은 `packetsPerSend`
- 장시간 비교도 필요하다.
  - `2시간`
  - `8시간`
- 가능하면 데스크탑 환경이 아닌 서버 운영 환경에 가까운 조건에서 다시 측정할 필요가 있다.
