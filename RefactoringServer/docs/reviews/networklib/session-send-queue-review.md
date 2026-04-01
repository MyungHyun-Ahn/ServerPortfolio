# NetworkLib Session Send Queue Review

## 1. 범위
- [`FSession.h`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FSession.h)
- [`FSession.cpp`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FSession.cpp)
- [`FSendBuffer.h`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FSendBuffer.h)
- [`FIocpServer.h`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FIocpServer.h)
- [`FIocpServer.cpp`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FIocpServer.cpp)

## 2. 변경 목적
- 기존 `Send()`는 호출마다 `WSASend`를 직접 걸어 단일 송신은 단순했지만, 같은 세션에서 짧은 시간에 여러 송신이 몰리면 송신 요청 수와 버퍼 수명주기 관리가 곧바로 복잡해진다.
- 레거시 프로젝트의 `N-Send` 의도처럼 "여러 송신 요청을 세션 단위 큐에 모은 뒤, 진행 중인 송신이 없을 때만 실제 `WSASend`를 건다"는 구조가 필요했다.
- 이 변경은 세션 단위 송신 큐와 단일 in-flight send 규칙을 도입해, 송신 경합을 lock-free enqueue + batched send 방식으로 정리하는 것이 목적이다.

## 3. 설계 요약
### 3-1. 세션 소유 송신 큐
- [`FSession`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FSession.h)는 [`FLockFreeQueue<FSendBuffer*>`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Containers\FLockFreeQueue.h) 기반의 `m_sendQueue`를 가진다.
- `Send()` 호출은 프레이밍과 암호화를 마친 뒤 [`FSendBuffer`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FSendBuffer.h)를 생성해서 세션 큐에 enqueue 한다.
- 이 단계는 I/O 호출을 직접 수행하지 않으므로, 여러 송신 요청이 들어와도 큐 적재는 비교적 짧은 경로로 끝난다.

### 3-2. 단일 in-flight send
- [`FSession::TryBeginSend()`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FSession.cpp)는 `m_sendInFlight`를 CAS로 전환한다.
- 이미 송신이 진행 중이면 추가 `WSASend`는 걸지 않고, 큐에 쌓인 상태로 남긴다.
- 송신 완료 시 [`FSession::EndSend()`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FSession.cpp)로 상태를 내리고, 곧바로 [`FIocpServer::PostSend()`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FIocpServer.cpp)을 다시 시도해 다음 배치를 이어서 보낸다.

### 3-3. Batched WSASend
- [`FSession::FillSendBatch()`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FSession.cpp)는 큐에서 최대 `kMaxSendBatchCount` 개의 버퍼를 꺼내 `m_activeSendBuffers`, `m_sendWsabufs`를 채운다.
- [`FIocpServer::PostSend()`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FIocpServer.cpp)은 이 `WSABUF` 배열을 한 번의 `WSASend`로 넘긴다.
- 이 구조로 송신 요청이 몰릴 때 시스템 콜 수를 줄이고, 레거시 `N-Send`의 "여러 패킷을 한 번에 묶어 보내는" 의도를 현재 구조에 맞게 가져왔다.

## 4. 판단 근거
### 4-1. 왜 세션이 송신 큐를 가져야 하는가
- 송신 큐가 서버 전역에 있으면 세션별 순서 보장과 종료 시 정리가 어려워진다.
- 세션 소유 큐로 두면 "이 큐에 든 버퍼는 이 세션 소켓으로만 나간다"는 소유권이 분명해진다.
- 세션 종료 시 [`FSession::ReleaseActiveSendBuffers()`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FSession.cpp), [`FSession::ReleaseQueuedSendBuffers()`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FSession.cpp)로 정리 경로도 한곳에 모인다.

### 4-2. 왜 lock-free queue를 선택했는가
- 여러 스레드에서 같은 세션으로 송신 요청이 몰릴 가능성을 고려하면, 짧은 enqueue 경로는 락보다 `Interlocked` 기반 큐가 현재 프로젝트 철학과 더 잘 맞는다.
- 이미 [`FLockFreeQueue`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Containers\FLockFreeQueue.h)는 별도 스트레스/soak 테스트를 통과했고, 이 프로젝트의 "lock-free 코어 유지" 방향과도 일치한다.

### 4-3. 왜 세션 소유 I/O context로 바꿨는가
- recv/send 모두 호출마다 I/O context를 새로 할당/해제하면 수명주기 추적이 번거롭고 실패 경로에서 정리 포인트가 많아진다.
- [`FSession::SIoContext`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FSession.h)를 세션이 소유하게 하여, recv/send 각각 하나의 context를 재사용하는 구조로 단순화했다.

## 5. 확인된 사실
- [`NetworkLib.vcxproj`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\NetworkLib.vcxproj) x64 Debug 빌드 성공
- [`EchoServer.vcxproj`](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\EchoServer.vcxproj) x64 Debug 빌드 성공
- [`EchoClient.vcxproj`](D:\Project\ServerPortfolio\RefactoringServer\EchoClient\EchoClient.vcxproj) x64 Debug 빌드 성공
- [`EchoServer.exe`](D:\Project\ServerPortfolio\RefactoringServer\Out\EchoServer.exe) `--headless` 실행 후 [`EchoClient.exe`](D:\Project\ServerPortfolio\RefactoringServer\Out\EchoClient.exe)로 아래 조건 검증 성공
  - `--count 8`
  - `--payload-size 48`
  - `--send-chunk-size 5`
  - `--send-chunk-delay-ms 1`
  - `--recv-buffer-size 11`
- 위 검증에서 8개 응답이 모두 정상 payload로 돌아왔고, 최종 출력은 `echo validation succeeded.` 였다.

## 6. 현재 한계
- 현재 `FSendBuffer`는 `new/delete` 기반이다. 송신 빈도가 매우 높아지면 전용 송신 버퍼 풀 또는 TLS 메모리 풀 연계가 필요할 수 있다.
- 현재 구조는 "세션당 한 번에 하나의 `WSASend`" 규칙을 둔다. 이 규칙은 단순성과 순서 보장에는 유리하지만, 고성능 최적화 단계에서는 더 공격적인 송신 파이프라인과 비교가 필요할 수 있다.
- 송신 큐 길이 제한이나 back-pressure 정책은 아직 없다. 큐가 과도하게 길어질 때의 운영 정책은 이후 별도 설계가 필요하다.

## 7. 다음 확인 항목
- `FSendBuffer`를 메모리 풀 기반으로 바꿨을 때의 효과 비교
- 세션 종료 직전 대량 enqueue 상황에서의 정리 경로 추가 검증
- 실제 게임 패킷 dispatch 구조가 올라왔을 때도 현재 세션 송신 큐 경계가 적절한지 재검토
