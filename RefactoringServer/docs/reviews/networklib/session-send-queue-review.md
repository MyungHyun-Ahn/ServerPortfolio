# NetworkLib Session Send Queue Review

## 1. 범위
- [FSession.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FSession.h)
- [FSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FSession.cpp)
- [FSendBuffer.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FSendBuffer.h)
- [FIocpServer.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FIocpServer.h)
- [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FIocpServer.cpp)
- [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)
- [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoClient\Main.cpp)

## 2. 변경 목적
- 같은 세션에 대해 여러 스레드가 동시에 `Send()`를 호출해도 실제 overlapped `WSASend`는 세션당 1개만 진행되게 만들 필요가 있었다.
- 레거시 프로젝트의 `N-Send` 의도처럼, 송신 요청은 세션 내부 큐에 쌓고 실제 I/O는 배치로 묶어서 보내는 구조가 목표였다.
- 장시간 검증을 위해 burst 테스트만이 아니라, 유지 시간 동안 지속적으로 요청/응답이 오가는 구조와 서버 측 통계 출력도 필요했다.

## 3. 설계 요약
### 3-1. 세션 소유 송신 큐
- [FSession](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FSession.h)은 `FLockFreeQueue<FSendBuffer*>` 기반 `m_sendQueue`를 가진다.
- [FIocpServer::Send](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FIocpServer.cpp)은 프레이밍과 암호화를 마친 버퍼를 즉시 `WSASend` 하지 않고 세션 큐에 넣는다.

### 3-2. 세션당 단일 in-flight send
- [FSession::TryBeginSend](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FSession.cpp)은 `m_sendInFlight`를 `false -> true`로 CAS 한다.
- 이미 send I/O가 진행 중이면 새 `WSASend`는 걸지 않고 큐에만 쌓인다.
- send 완료 시 [FSession::EndSend](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FSession.cpp)로 플래그를 내리고 다음 배치를 다시 시도한다.

### 3-3. 런타임 검증 가드
- [FSession](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FSession.h)에 `m_liveSendIoCount`, `m_maxObservedConcurrentSendIoCount`를 두었다.
- [FIocpServer::PostSend](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FIocpServer.cpp)에서 실제 `WSASend` 직전 `BeginSendIo()`를 호출한다.
- 동시에 2개 이상 send I/O가 잡히면 `Concurrent WSASend detected` 로그를 남기고 세션을 종료한다.
- 세션 종료 시 `maxConcurrentSendIo=`를 로그에 남겨 실제 관측 최대값을 확인할 수 있다.

### 3-4. Batched WSASend
- [FSession::FillSendBatch](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FSession.cpp)은 큐에서 최대 `kMaxSendBatchCount`개를 꺼내 `WSABUF[]`를 만든다.
- [FIocpServer::PostSend](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FIocpServer.cpp)은 이 배열을 한 번의 `WSASend`로 넘긴다.

### 3-5. 장시간 송수신 검증용 통계
- [IServer.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\IServer.h)에 `GetStatsSnapshot()`을 추가했다.
- [FIocpServer](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FIocpServer.cpp)는 아래를 원자 카운터로 집계한다.
  - 누적 accept 수
  - 활성 세션 수
  - 수신 패킷 수
  - 송신 패킷 수
  - `WSARecv` 호출 수
  - `WSASend` 호출 수
- [EchoServer/Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)는 `--headless` 실행 중 1초마다 `EchoStats`를 콘솔에 출력한다.
- 현재 출력 항목:
  - `acceptTPS`
  - `recvTPS`
  - `sendTPS`
  - `wsaRecvTPS`
  - `wsaSendTPS`
  - `totalWSARecvCalls`
  - `totalWSASendCalls`

## 4. 클라이언트 검증 보강
- [EchoClient/Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoClient\Main.cpp)는 이제 한 프로세스에서 여러 세션을 유지할 수 있다.
- 주요 옵션:
  - `--sessions`
  - `--hold-seconds`
  - `--interval-ms`
  - `--packets-per-send`
  - `--reconnect-probability-percent`
  - `--reconnect-delay-ms`
- 각 세션은 유지 시간 동안 주기적으로 요청을 보내고, 응답 집합 전체를 검증한다.
- 따라서 이전의 "burst 후 idle"이 아니라 실제로 장시간 `Send/Recv`가 반복되는 검증이 가능해졌다.
- `--packets-per-send`는 여러 프레임을 하나의 `send()` 호출에 이어붙여 보내므로, 애플리케이션 패킷 수와 소켓 send 호출 수를 분리해서 볼 수 있다.
- `--reconnect-probability-percent`는 주기 사이클마다 일정 확률로 연결을 끊고 다시 붙게 하므로, accept/close 반복 경로를 장시간 검증할 수 있다.

## 5. 확인된 사실
### 5-1. 기본 멀티스레드 send 검증
- 서버:
  - [EchoServer.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\EchoServer.exe)
  - `--headless --send-thread-count 4 --responses-per-thread 4`
- 클라이언트:
  - [EchoClient.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\EchoClient.exe)
  - `--count 8 --payload-size 48 --send-chunk-size 5 --send-chunk-delay-ms 1 --recv-buffer-size 11 --response-thread-count 4 --responses-per-thread 4`
- 결과:
  - 총 `128` 응답 성공
  - `echo validation succeeded.` 확인
  - `Concurrent WSASend detected` 로그 미발생

### 5-2. 더 강한 burst 검증
- 서버:
  - `--headless --send-thread-count 8 --responses-per-thread 8`
- 클라이언트:
  - `--count 16 --payload-size 64 --send-chunk-size 7 --send-chunk-delay-ms 1 --recv-buffer-size 13 --response-thread-count 8 --responses-per-thread 8 --quiet`
- 결과:
  - 총 `1024` 응답 성공
  - `echo validation succeeded.` 확인
  - 세션 종료 로그에서 `maxConcurrentSendIo=1` 확인

### 5-3. 지속 송수신 검증
- 서버:
  - `--headless --send-thread-count 4 --responses-per-thread 4`
- 클라이언트:
  - `--sessions 4 --count 2 --payload-size 32 --send-chunk-size 4 --send-chunk-delay-ms 1 --recv-buffer-size 9 --response-thread-count 4 --responses-per-thread 4 --hold-seconds 3 --interval-ms 500 --quiet`
- 결과:
  - 총 `640` 응답 성공
  - 서버 콘솔에 아래 통계 출력 확인

```text
[EchoStats] sessions=4 recvTPS=8 sendTPS=128 WSASendCalls=16 WSARecvCalls=81
[EchoStats] sessions=4 recvTPS=12 sendTPS=192 WSASendCalls=52 WSARecvCalls=212
[EchoStats] sessions=4 recvTPS=12 sendTPS=192 WSASendCalls=88 WSARecvCalls=321
```

- 종료 로그에서도 세션별 `maxConcurrentSendIo=1` 확인

### 5-4. 패킷 묶음 송신 + 확률 재접속 검증
- 서버:
  - [EchoServer.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\EchoServer.exe)
  - `--headless --send-thread-count 1 --responses-per-thread 1`
- 클라이언트:
  - [EchoClient.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\EchoClient.exe)
  - `--sessions 8 --count 8 --payload-size 48 --packets-per-send 4 --send-chunk-size 8 --send-chunk-delay-ms 1 --recv-buffer-size 16 --response-thread-count 1 --responses-per-thread 1 --hold-seconds 3 --interval-ms 500 --reconnect-probability-percent 30 --reconnect-delay-ms 50 --quiet`
- 결과:
  - 총 `192` 응답 성공
  - 확률 재접속으로 `acceptTPS`가 증가하는 구간 확인
  - 서버 출력 예시:

```text
[EchoStats] sessions=8 acceptTPS=3 recvTPS=38 sendTPS=38 wsaSendTPS=38 wsaRecvTPS=247 totalWSASendCalls=135 totalWSARecvCalls=941
[EchoStats] sessions=0 acceptTPS=0 recvTPS=57 sendTPS=57 wsaSendTPS=57 wsaRecvTPS=363 totalWSASendCalls=192 totalWSARecvCalls=1304
```

  - `1:1 echo` 모드에서는 `recvTPS`와 `sendTPS`가 같게 나오는 것 확인
  - 세션 종료 로그에서 `maxConcurrentSendIo=1` 유지 확인

## 6. 판단
- 현재 구조는 "여러 스레드의 동시 `Send()` 호출"과 "세션당 단일 in-flight `WSASend`" 규칙을 양립시키는 데 성공했다.
- 검증 근거는 단순 응답 성공만이 아니라, 실제 런타임 가드와 `maxConcurrentSendIo` 계측까지 포함한다.
- 지속 송수신 상태에서 TPS와 `WSASend`/`WSARecv` 호출 수를 콘솔로 관측할 수 있으므로 장시간 테스트 기반도 마련됐다.
- `packets-per-send`와 확률 재접속 옵션이 추가되어, 패킷 배치 송신과 accept/close 반복도 같은 테스트 클라이언트로 검증할 수 있다.

## 7. 남은 확인 항목
- 100세션 이상, 2시간 이상 장시간 실행 로그를 기준으로 최종 안정성 검증
- `FSendBuffer`를 메모리 풀 기반으로 바꿨을 때의 성능 비교
- 너무 긴 송신 큐가 쌓일 때의 back-pressure 정책
