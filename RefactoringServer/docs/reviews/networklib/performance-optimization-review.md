# Performance Optimization Review

## Page Pool Benchmark Result
- Conclusion:
  - In the current fixed-workload Echo benchmark, `page pool on` performed about `4% to 5%` better than `page pool off`.
- Benchmark scenario:
  - `sessions=50`
  - `requestCount=64`
  - `payloadSize=128`
  - `packetsPerSend=4`
  - `holdSeconds=0`
  - `intervalMs=0`
  - `sendChunkSize=7`
  - `recvBufferSize=13`
- Measured result:
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
- Interpretation:
  - Under the current Echo scenario, enabling page-sized buffer reuse improved throughput slightly and consistently.
  - The difference is not huge, so this should be treated as a practical optimization rather than a structural breakthrough.
  - For a stronger conclusion, the same fixed-workload test should be repeated `3 to 5` times and averaged.
- Evidence logs:
  - [echo_server_perf_20260402_105058.log](D:\Project\ServerPortfolio\RefactoringServer\Out\perf\echo_server_perf_20260402_105058.log)
  - [echo_server_perf_20260402_105117.log](D:\Project\ServerPortfolio\RefactoringServer\Out\perf\echo_server_perf_20260402_105117.log)

## TODO
- Long-run performance validation is still required.
- Next step:
  - run the same page pool `on/off` comparison for `2 hours` and `8 hours`
  - compare average TPS, bytes/sec, CPU usage, and pool usage stability
  - check whether the short-run `4% to 5%` advantage remains stable over time

## Page Pool Option
- Page-sized reuse is now intended to be an explicit option instead of a forced behavior.
- Server-side configuration:
  - `SServerConfig.enablePageBufferReuse`
  - `SServerConfig.pageBufferSize`
- Sample client configuration:
  - `--disable-page-pool`
  - `--page-size <bytes>`
- This keeps benchmark comparisons fair because the same scenario can be run with page reuse on and off.

## 1. 목적
- `NetworkLib` 성능 고도화 작업의 현재 적용 상태를 한 문서에서 파악할 수 있게 정리한다.

## 2. 이번 단계에 적용한 것

### 2-1. 버퍼 재사용
- `FSession` TLS pool 재사용
- `FSendBuffer` TLS pool 재사용
- `FPacketWriter` 내부 `FPacketBuffer` TLS pool 재사용
- `FPacketBuffer`, `FSendBuffer`는 기본적으로 4KB page-sized capacity를 유지하도록 조정했다.

### 2-2. 계측
- 서버 통계에 아래를 추가했다.
  - `receivedByteCount`
  - `sentByteCount`
  - `queuedSendBufferCount`
  - `maxObservedQueuedSendBufferCount`
  - `sessionPoolUsage/capacity`
  - `sendBufferPoolUsage/capacity`
  - `packetBufferPoolUsage/capacity`

### 2-3. 샘플 콘텐츠 경량화
- `EchoServer`의 로그인 상태 추적을 `unordered_map + mutex`에서 세션 슬롯 기반 `vector<atomic<uint32_t>>`로 바꿨다.
- 이 변경은 샘플 서버의 콘텐츠 상태 경량화 예시다.

### 2-4. 벤치마크 실행 경로
- [Run-EchoPerfBenchmark.ps1](D:\Project\ServerPortfolio\RefactoringServer\scripts\Run-EchoPerfBenchmark.ps1)
- [Run-EchoPerfBenchmark.cmd](D:\Project\ServerPortfolio\RefactoringServer\scripts\Run-EchoPerfBenchmark.cmd)

## 3. 현재 서버 콘솔에서 볼 수 있는 값
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

## 4. 검증 근거
- 빌드:
  - `RefactoringServer.sln` x64 Debug
- 테스트:
  - [LockFreeTests.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\LockFreeTests.exe)
- 런타임:
  - [EchoServer.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\EchoServer.exe)
  - [EchoClient.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\EchoClient.exe)

## 5. 다음 측정 작업
- baseline / candidate를 같은 시나리오로 2회 이상 반복 측정
- `perf` 로그를 기준으로 평균값을 표로 정리
- page pool 본체 도입 필요 여부를 수치로 판단
