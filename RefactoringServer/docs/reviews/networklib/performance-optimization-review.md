# 성능 최적화 리뷰

## 1. 목적
- `NetworkLib` 성능 고도화 작업의 현재 적용 상태와 측정 결과를 한 문서에서 확인할 수 있도록 정리한다.

## 2. 이번 단계에서 적용한 내용
### 2-1. 버퍼 재사용
- `FSession`에 TLS 풀을 적용했다.
- `FSendBuffer`에 TLS 풀을 적용했다.
- `FPacketWriter` 내부 버퍼인 `FPacketBuffer`에 TLS 풀을 적용했다.
- `FPacketBuffer`, `FSendBuffer`는 page 성격의 capacity 재사용이 가능하도록 정리했다.

### 2-2. 계측 추가
- 서버 통계에 아래 항목을 추가했다.
  - `receivedByteCount`
  - `sentByteCount`
  - `queuedSendBufferCount`
  - `maxObservedQueuedSendBufferCount`
  - `sessionPoolUsage/capacity`
  - `sendBufferPoolUsage/capacity`
  - `packetBufferPoolUsage/capacity`

### 2-3. 세션/컨텍스트 경량화
- `EchoServer`의 로그인 상태 관리를 `unordered_map + mutex`에서 세션 슬롯 기반 `vector<atomic<uint32_t>>`로 변경했다.
- 이 변경은 샘플 서버 기준의 콘텐츠 상태 경량화 예시다.

### 2-4. 벤치마크 실행 경로
- [Run-EchoPerfBenchmark.ps1](D:\Project\ServerPortfolio\RefactoringServer\scripts\Run-EchoPerfBenchmark.ps1)
- [Run-EchoPerfBenchmark.cmd](D:\Project\ServerPortfolio\RefactoringServer\scripts\Run-EchoPerfBenchmark.cmd)

## 3. Page Pool 옵션
- page 성격의 버퍼 재사용은 강제가 아니라 옵션이다.
- 서버 설정:
  - `SServerConfig.enablePageBufferReuse`
  - `SServerConfig.pageBufferSize`
- 샘플 클라이언트 실행 옵션:
  - `--disable-page-pool`
  - `--page-size <bytes>`
- 따라서 같은 시나리오를 page pool `on/off`로 공정하게 비교할 수 있다.

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
- 현재 고정 작업량 Echo 벤치마크 기준으로는 `page pool on`이 `off`보다 대략 `4% ~ 5%` 정도 더 좋게 나왔다.
- 차이가 압도적이지는 않으므로, 구조적 돌파라기보다 실용적인 최적화로 보는 편이 맞다.
- 장기적인 판단을 위해서는 같은 조건으로 반복 측정과 장시간 비교가 더 필요하다.

### 4-4. 근거 로그
- [echo_server_perf_20260402_105058.log](D:\Project\ServerPortfolio\RefactoringServer\Out\perf\echo_server_perf_20260402_105058.log)
- [echo_server_perf_20260402_105117.log](D:\Project\ServerPortfolio\RefactoringServer\Out\perf\echo_server_perf_20260402_105117.log)

## 5. 현재 서버 콘솔에서 볼 수 있는 통계
- `acceptTPS`
- `recvTPS`
- `sendTPS`
- `recvBps`
- `sendBps`
- `wsaSendTPS`
- `wsaRecvTPS`
- `queuedSendBuffers`
- `maxQueuedSendBuffers`
- `sessionPool`
- `sendBufferPool`
- `packetBufferPool`

## 6. 검증 근거
- 빌드:
  - `RefactoringServer.sln` x64 Debug
- 테스트:
  - [LockFreeTests.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\LockFreeTests.exe)
- 런타임:
  - [EchoServer.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\EchoServer.exe)
  - [EchoClient.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\EchoClient.exe)

## 7. TODO
- 장시간 성능 검증이 아직 필요하다.
- 다음 단계:
  - 같은 page pool `on/off` 비교를 `2시간`, `8시간`으로 반복
  - 평균 TPS, bytes/sec, CPU 사용량, pool usage 안정성 비교
  - 짧은 벤치마크에서 나온 `4% ~ 5%` 우위가 장시간에도 유지되는지 확인
