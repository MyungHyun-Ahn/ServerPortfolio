# 네트워크 성능 최적화 리뷰

## 1. 목적
- `NetworkLib` 성능 고도화 작업의 적용 상태와 측정 결과를 한 문서에서 확인할 수 있도록 정리한다.

## 2. 이번 단계에서 적용한 내용
### 2-1. 버퍼 재사용
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
- 샘플 서버 기준에서 콘텐츠 상태 접근 비용을 줄이는 방향의 초안으로 본다.

### 2-4. 벤치마크 실행 경로
- [Run-EchoPerfBenchmark.ps1](D:\Project\ServerPortfolio\RefactoringServer\scripts\Run-EchoPerfBenchmark.ps1)
- [Run-EchoPerfBenchmark.cmd](D:\Project\ServerPortfolio\RefactoringServer\scripts\Run-EchoPerfBenchmark.cmd)

## 3. Page Pool 옵션
- page 성격 버퍼 재사용은 강제가 아니라 옵션이다.
- 서버 설정:
  - `SServerConfig.enablePageBufferReuse`
  - `SServerConfig.pageBufferSize`
- 샘플 실행 옵션:
  - `--disable-page-pool`
  - `--page-size <bytes>`

## 4. Page Pool 성능 비교 결과
### 4-1. 측정 조건
- `sessions=50`
- `requestCount=64`
- `payloadSize=128`
- `packetsPerSend=4`
- `holdSeconds=0`
- `intervalMs=0`
- `sendChunkSize=7`
- `recvBufferSize=13`

### 4-2. 측정 결과
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

### 4-3. 해석
- 현재 고정 작업량 Echo 벤치마크 기준으로 `page pool on`이 `off`보다 대략 `4% ~ 5%` 정도 더 좋았다.
- 차이가 아주 크진 않지만, 구조 복잡도를 감안해도 유지할 가치가 있는 수준으로 본다.

### 4-4. 근거 로그
- [echo_server_perf_20260402_105058.log](D:\Project\ServerPortfolio\RefactoringServer\Out\perf\echo_server_perf_20260402_105058.log)
- [echo_server_perf_20260402_105117.log](D:\Project\ServerPortfolio\RefactoringServer\Out\perf\echo_server_perf_20260402_105117.log)

## 5. BuildPacket 복사 감소 적용 결과
### 5-1. 적용 내용
- send 경로에서 `payloadBuffer -> framedBuffer`로 한 번 더 복사하는 경로를 줄였다.
- 현재는 `FSendBuffer`가 아래 두 조각을 send completion 시점까지 소유한다.
  - frame header 조각
  - payload 조각
- 즉 `1-session 1 in-flight WSASend` 규칙은 유지하면서 중간 버퍼 복사를 줄이는 방향으로 정리했다.

### 5-2. 비교 조건
- `sessions=50`
- `requestCount=64`
- `payloadSize=128`
- `packetsPerSend=4`
- `holdSeconds=0`
- `intervalMs=0`
- `sendChunkSize=7`
- `recvBufferSize=13`

### 5-3. 측정 결과
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

### 5-4. 해석
- 현재 Echo 고정 작업량 시나리오에서는 `BuildPacket` 복사 감소만으로 눈에 띄는 TPS 개선은 확인되지 않았다.
- 구조상 복사 수는 줄었지만, 이 시나리오에서는 병목이 다른 지점이거나 개선 폭이 측정 노이즈보다 작은 것으로 보인다.

### 5-5. 근거 로그
- 변경 전
  - [echo_server_perf_20260402_113537.log](D:\Project\ServerPortfolio\RefactoringServer\Out\perf\echo_server_perf_20260402_113537.log)
  - [echo_server_perf_20260402_113559.log](D:\Project\ServerPortfolio\RefactoringServer\Out\perf\echo_server_perf_20260402_113559.log)
- 변경 후
  - [echo_server_perf_20260402_113809.log](D:\Project\ServerPortfolio\RefactoringServer\Out\perf\echo_server_perf_20260402_113809.log)
  - [echo_server_perf_20260402_113831.log](D:\Project\ServerPortfolio\RefactoringServer\Out\perf\echo_server_perf_20260402_113831.log)

## 6. CPU / 메모리 계측 확인
### 6-1. 추가한 항목
- `cpuPercent`
- `workingSetMB`
- `peakWorkingSetMB`

### 6-2. 스모크 확인 결과
- headless 서버 출력에서 위 세 항목이 실제로 1초 통계에 포함되는 것을 확인했다.
- 예시:
  - `cpuPercent=0.19`
  - `workingSetMB=7.84`
  - `peakWorkingSetMB=7.84`

### 6-3. 근거 로그
- [cpu_mem_smoke_server.log](D:\Project\ServerPortfolio\RefactoringServer\Out\cpu_mem_smoke_server.log)
- [cpu_mem_smoke_client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\cpu_mem_smoke_client.log)

## 7. 검증 근거
- 빌드:
  - `RefactoringServer.sln` x64 Debug
- 테스트:
  - [LockFreeTests.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\LockFreeTests.exe)
- 실행:
  - [EchoServer.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\EchoServer.exe)
  - [EchoClient.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\EchoClient.exe)

## 8. TODO
- 장시간 성능 검증이 아직 필요하다.
- 다음 단계:
  - 같은 page pool `on/off` 비교를 `2시간`, `8시간`으로 반복
  - 평균 TPS, bytes/sec, CPU 사용량, working set, pool usage 안정성 비교
  - 지금 벤치마크에서 확인한 `4% ~ 5%` 차이가 장시간에도 유지되는지 확인
  - `BuildPacket` 복사 감소 적용분을 더 고부하 시나리오에서 재측정
  - 더 많은 세션 수, 더 큰 payload, 더 높은 `packetsPerSend` 조건 추가
