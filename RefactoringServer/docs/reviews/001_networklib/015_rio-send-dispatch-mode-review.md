# RIO Send Dispatch Mode Review

## 1. 목적
- `Backend: Rio`를 유지한 채 send 경로를 `Direct`와 `OwnerThread` 두 방식으로 전환할 수 있도록 확장한 내용을 정리한다.
- 이 문서는 아래 질문에 답하는 것을 목표로 한다.
  - `RioSendDispatchMode`는 어디서 설정되고 어떻게 서버 설정으로 반영되는가
  - `Direct`와 `OwnerThread`는 실제로 어떤 코드 경로 차이가 있는가
  - owner worker가 현재 구조에서 어디까지 ownership을 가지는가
  - A/B 비교를 하기 위한 기준선은 어떻게 검증했는가

## 2. 변경 목적
- 기존 pure `RIO` baseline은 recv completion은 owner worker 중심으로 처리하지만, send는 호출한 스레드에서 바로 `RIOSend()`를 호출했다.
- 이 구조는 baseline으로는 단순하지만, 세션 hot path를 owner worker로 더 모을 수 있는지 비교하기 어렵다.
- 그래서 `FRioServer` 내부에 send dispatch policy를 추가하고, 같은 backend 안에서 아래 두 모드를 비교할 수 있게 만들었다.
  - `Direct`
  - `OwnerThread`

## 3. 코드 구조
### 3-1. 설정 경로
- 서버 schema: [EchoServer.schema.yaml](D:\Project\ServerPortfolio\RefactoringServer\ConfigSchema\Server\EchoServer.schema.yaml)
- generated config: [EchoServerConfig.h](D:\Project\ServerPortfolio\RefactoringServer\Generated\Config\EchoServer\EchoServerConfig.h)
- sample YAML: [EchoServer.yaml](D:\Project\ServerPortfolio\RefactoringServer\Config\Server\EchoServer.yaml)
- 서버 설정 변환: [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)
- 공용 서버 설정: [BackendTypes.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\BackendTypes.h)

### 3-2. RIO 서버 경로
- 서버 구현: [FRioServer.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.h)
- 서버 구현: [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp)
- 세션 구현: [FRioSession.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FRioSession.h)
- 세션 구현: [FRioSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FRioSession.cpp)

## 4. 설정 방식
`EchoServer.yaml`에서 다음 값을 사용한다.

```yaml
EchoServer:
  Backend: Rio
  RioSendDispatchMode: Direct
```

또는:

```yaml
EchoServer:
  Backend: Rio
  RioSendDispatchMode: OwnerThread
```

설정 반영 흐름:
1. generated loader가 `RioSendDispatchMode`를 enum으로 읽는다.
2. [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)의 `ToRioSendDispatchMode(...)`가 generated enum을 `NetworkLib::Core::ERioSendDispatchMode`로 변환한다.
3. `serverConfig.rioSendDispatchMode`에 저장된다.
4. [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp)의 `Send(...)`가 이 값을 보고 분기한다.

CLI override도 추가되어 있다.
- `--rio-send-dispatch-mode direct`
- `--rio-send-dispatch-mode owner`
- `--rio-send-dispatch-mode ownerthread`

## 5. Direct 모드 흐름
`Direct`는 기존 pure `RIO` baseline 동작을 유지한다.

흐름:
1. application/content thread가 `IServer::Send(...)`를 호출한다.
2. [FRioServer::Send](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp)가 session을 찾고 framed packet을 만든다.
3. `SubmitSendDirect(...)`로 들어간다.
4. `FPacketBuffer`를 `RIORegisterBuffer()`로 등록한다.
5. session의 request queue mutex를 잠근 뒤 바로 `RIOSend()`를 호출한다.
6. send completion은 owner worker의 CQ에서 dequeue되고, `HandleSendCompletion(...)`이 정리한다.

특징:
- 호출 스레드가 직접 send submit을 수행한다.
- worker inbox handoff가 없다.
- baseline 비교 대상이다.

## 6. OwnerThread 모드 흐름
`OwnerThread`는 send submit도 owner worker가 하도록 경로를 나눈다.

흐름:
1. application/content thread가 `IServer::Send(...)`를 호출한다.
2. [FRioServer::Send](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp)가 session을 찾고 framed packet을 만든다.
3. session의 `ownerWorkerIndex`를 읽는다.
4. `EnqueueOwnerThreadSend(...)`가 owner worker의 `sendCommands` 큐에 `SSendCommand`를 넣는다.
5. worker의 `completionEvent`를 `SetEvent(...)`로 깨운다.
6. owner worker의 [WorkerLoop](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp)가 `DrainSendCommands(workerIndex)`를 호출한다.
7. `DrainSendCommands(...)`가 command를 dequeue하고, 실제 `SubmitSendDirect(...)`를 owner worker 스레드에서 호출한다.
8. 이후 send completion은 같은 owner worker CQ에서 dequeue되고 `HandleSendCompletion(...)`이 정리한다.

특징:
- send submit 경로가 owner worker로 집중된다.
- cross-thread handoff 1회가 추가된다.
- 완전 owner-thread 모델에 가까운 send path 비교용이다.

## 7. 현재 ownership 모델
이 변경 이후 구조를 정확히 표현하면 아래와 같다.

- recv completion 소비: owner worker
- send completion 소비: owner worker
- `OwnerThread` 모드의 send submit: owner worker
- `Direct` 모드의 send submit: 호출 스레드

즉:
- `Direct`는 부분 owner-thread 구조
- `OwnerThread`는 send까지 owner worker로 모은 구조

아직 완전 owner-thread라고 부르지 않는 이유:
- accept는 accept thread가 처리한다.
- connect/disconnect 요청 시작점은 owner worker가 아닐 수 있다.
- 관리 plane과 lifecycle 일부는 여전히 다른 스레드가 시작할 수 있다.

하지만 `OwnerThread` 모드는 적어도 send hot path를 owner worker로 모으는 비교 실험에는 적합하다.

## 8. worker 내부 자료구조
[FRioServer.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.h)의 `SRioWorker`에는 아래가 추가되었다.

- `std::mutex sendCommandMutex`
- `std::deque<SSendCommand> sendCommands`

설계 의도:
- 첫 구현은 단순하고 안전하게 간다.
- 정확성 확보 후 필요 시 hot path lock-free 전환 여부를 별도로 측정한다.

즉 이번 단계는:
- `lock을 허용한 owner-thread send baseline`
- 이후 필요 시 `lock-free owner inbox`로 확장

## 9. shutdown / 정리 경로
`OwnerThread` 모드에서는 worker 큐에 남은 `FPacketBuffer`가 leak 되지 않도록 정리 경로가 추가되었다.

- [FRioServer::StopWorkers](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp)
  - worker thread join 후 `sendCommands`를 swap하여 남은 buffer를 release한다.

또한 worker loop의 종료 조건은 pending send command도 본다.
- `!HasPendingSendCommands(workerIndex)`

이유:
- CQ가 비어도 owner send queue가 남아 있으면 worker를 먼저 종료하면 안 되기 때문이다.

## 10. 검증 결과
이번 변경에서 확인한 기준선:

### 10-1. build
- `Debug x64` 솔루션 빌드 성공

### 10-2. 짧은 스모크
- `Rio + Direct`, `20세션 / 15초` 성공
  - [client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_send_dispatch_direct_smoke\client.log)
- `Rio + OwnerThread`, `20세션 / 15초` 성공
  - [client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_send_dispatch_owner_smoke\client.log)

### 10-3. 짧은 회귀
- `Rio + Direct`, `100세션 / 60초` 성공
  - [client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_send_dispatch_direct_100x60\client.log)
- `Rio + OwnerThread`, `100세션 / 60초` 성공
  - [client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_send_dispatch_owner_100x60\client.log)

현재 의미:
- 비교용 코드 경로는 둘 다 정상 동작한다.
- 이제 성능 비교는 구조 correctness가 아니라 throughput/latency 관점의 A/B로 진행하면 된다.

### 10-4. 10분 A/B 측정 결과
비교 조건:
- `250세션`
- `holdSeconds=600`
- `room-count=80`
- `room-capacity=4`
- `room-change=90%`
- recv 관련 timeout `15000ms`

로그:
- `Rio + Direct`
  - [client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_direct_fix_250x10m_t15_r80\client.log)
  - [server.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_direct_fix_250x10m_t15_r80\server.log)
  - [rtt.csv](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_direct_fix_250x10m_t15_r80\rtt.csv)
- `Rio + OwnerThread`
  - [client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_owner_fix_250x10m_t15_r80\client.log)
  - [server.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_owner_fix_250x10m_t15_r80\server.log)
  - [rtt.csv](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_owner_fix_250x10m_t15_r80\rtt.csv)
- `Iocp`
  - [client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\iocp_fix_250x10m_t15_r80\client.log)
  - [server.log](D:\Project\ServerPortfolio\RefactoringServer\Out\iocp_fix_250x10m_t15_r80\server.log)
  - [rtt.csv](D:\Project\ServerPortfolio\RefactoringServer\Out\iocp_fix_250x10m_t15_r80\rtt.csv)

참고 수치:

| Mode | responses total | echo-response avg | room-change-list avg | room-change avg |
| --- | ---: | ---: | ---: | ---: |
| Rio Direct | 619436 | 3.584 ms | 8.107 ms | 9.390 ms |
| Rio OwnerThread | 664640 | 34.702 ms | 41.093 ms | 69.929 ms |
| Iocp | 669984 | 4.228 ms | 9.256 ms | 12.053 ms |

해석:
- `OwnerThread`는 현재 구현 기준으로 처리량은 약간 높게 나왔지만 RTT 평균은 크게 악화됐다.
- `Direct`와 `Iocp`는 RTT가 비슷한 수준이고, 이번 1회 측정에서는 `Iocp` 처리량이 가장 높았다.
- 따라서 현재 상태에서 `OwnerThread`는 “비교용 실험 모드”로는 유효하지만, 기본 모드로 채택하기엔 지연 손해가 크다.
- 다만 여기서의 처리량 평가는 `responses total` 기준의 10분 파일럿이다.
- 2시간 본실험부터는 `overall avg recvTPS/sendTPS/Bps`를 주 비교값으로 사용해야 한다.

### 10-5. 2시간 본실험 결과
비교 조건:
- 순서: `RioDirect -> RioOwnerThread -> Iocp`
- `250세션`
- `holdSeconds=7200`
- `room-count=80`
- `room-capacity=4`
- `room-change=90%`
- recv 관련 timeout `15000ms`

결과:
- 요약 CSV: [summary.csv](D:\Project\ServerPortfolio\RefactoringServer\Out\dispatch_ab_2h_20260405_035034_2h\summary.csv)
- 세 모드 모두 성공
  - `client.err.log` 비어 있음
  - 2시간 hold 종료 후 정상 정리

핵심 수치:

| Mode | responses total | avg recvTPS | avg sendTPS | avg recvBps | avg sendBps | avg CPU | echo avg | room-change-list avg | room-change avg |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Rio Direct | 1768780 | 714.084 | 714.084 | 10199.501 | 456290.961 | 3.766% | 1.578 ms | 1.446 ms | 4.647 ms |
| Rio OwnerThread | 1767533 | 711.470 | 711.370 | 10168.318 | 454156.445 | 3.729% | 1.470 ms | 1.459 ms | 4.778 ms |
| Iocp | 1767073 | 710.884 | 710.884 | 10164.527 | 453679.689 | 3.700% | 1.464 ms | 1.418 ms | 4.651 ms |

해석:
- `avg TPS/Bps` 기준으로는 `Rio Direct`가 가장 높았다.
- `OwnerThread`는 10분 파일럿에서 보였던 처리량 우세가 2시간 avg에선 유지되지 않았다.
- `echo-response avg`는 `Iocp`와 `Rio OwnerThread`가 근소하게 낮았지만 차이는 매우 작다.
- `room-change-list avg`는 `Iocp`가 가장 낮고, `Rio Direct`가 근접했다.
- `room-change avg`는 `Rio Direct`와 `Iocp`가 사실상 같은 수준이고, `OwnerThread`가 약간 더 느렸다.
- 따라서 현재 구현 상태에선 `OwnerThread`를 기본값으로 채택할 근거가 부족하다.

## 11. 주의사항
1. `OwnerThread` 모드는 구조 비교용이지, 아직 최종 승자라고 판단한 것은 아니다.
2. worker inbox는 현재 mutex + deque다.
   - 비교 목적상 단순성과 안전성을 우선했다.
3. `Direct`가 더 빠를 가능성도 열려 있다.
   - owner handoff가 추가 latency가 될 수 있기 때문이다.
4. `OwnerThread`가 더 나을 가능성도 있다.
   - send ownership 집중으로 cache locality와 동기화 단순성이 좋아질 수 있다.
5. 실제 10분 측정에서도 현재 구현은 이 trade-off가 드러났다.
   - 처리량은 약간 좋아졌지만 RTT 평균은 크게 나빠졌다.
6. 따라서 다음 단계는 최적화가 아니라 측정이다.
   - [011_rio-owner-thread-ab-benchmark.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance\011_rio-owner-thread-ab-benchmark.md)
7. 2시간 본실험까지 보면 현재 기본값 후보는 `Rio Direct`다.
   - `OwnerThread`는 후속 최적화 실험 경로로 남겨두는 편이 맞다.

## 12. 결론
- `FRioServer`는 이제 `RioSendDispatchMode` 설정 하나로 `Direct`와 `OwnerThread` 두 send 정책을 전환할 수 있다.
- 기존 pure `RIO` baseline은 유지된다.
- owner-thread send 비교 경로도 같은 backend 안에서 실험 가능하다.
- correctness 기준선은 확보되었고, 2시간 A/B 측정도 끝났다.
- 현재 결론은 `Rio`를 쓸 경우 기본 send 정책은 `Direct` 유지가 적절하다는 것이다.
