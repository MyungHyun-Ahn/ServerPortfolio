# Send Post Lost-Wakeup 리뷰

## 1. 범위
- 이 문서는 `Lobby -> Room -> RoomChange -> Echo` 검증 중 발견한 `send queue가 1개 남은 채 더 이상 진행되지 않는 hang`만 따로 정리한다.
- 콘텐츠 전이 규칙이나 `echo-response timeout` 전체 이슈와 분리해서, `NetworkLib` send 경로 자체의 문제만 본다.

## 2. 증상
- `recv timeout`을 모두 끄고 1분 런을 돌렸는데도 클라이언트가 끝나지 않고 멈췄다.
- 외부 watchdog이 90초 뒤 강제 종료했다.
- 서버 상태는 다음과 같았다.
  - `sessions=1`
  - `recvTPS=0`
  - `sendTPS=0`
  - `queuedSendBuffers=1`
  - `totalWSASendCalls` 증가 없음
- 즉 마지막 세션 하나가 살아 있고, send queue에는 버퍼가 남아 있는데 `WSASend`가 다시 걸리지 않는 상태였다.

## 3. 원인
### 3.1 레거시와 달랐던 점
- 레거시 프로젝트는 `SendPacket()`과 `EnqueuePacket()`을 분리하고 있었다.
- 더 중요한 점은 `m_iSendFlag + ENQUEUE_FLAG`로 `send 중 새 enqueue가 들어오면 다음 PostSend를 놓치지 않도록` 보장하고 있었다.
- 현재 `RefactoringServer`는 이 부분이 `std::atomic<bool> m_sendInFlight`로 단순화되어 있었다.

### 3.2 현재 코드에서 가능했던 race
1. send completion 쪽이 `PostSend()`에 들어가 `m_sendInFlight=true`를 잡는다.
2. queue를 확인했더니 비어 있어서 종료하려고 한다.
3. 그 사이 다른 스레드가 새 send buffer를 enqueue하고 `PostSend()`를 호출한다.
4. 하지만 `m_sendInFlight=true`라서 두 번째 `PostSend()`는 그냥 빠진다.
5. 첫 번째 `PostSend()`는 `m_sendInFlight=false`로 내리고 끝난다.
6. 결과적으로 queue에는 버퍼가 남았는데 send가 다시 시작되지 않는다.

## 4. 조사 결과
- 현재 `RefactoringServer`에는 레거시의 `EnqueuePacket()` 같은 `enqueue-only network send` API는 없었다.
- 즉 이번 문제는 `특수 API를 잘못 사용해서 PostSend를 안 불렀다`가 아니라, `send 재기동 보장 자체가 빠져 있었다`가 맞다.
- 이 패턴은 실제 로그와도 잘 맞았다.
  - `queuedSendBuffers=1`
  - `sendTPS=0`
  - `totalWSASendCalls` 정지
  - 클라이언트는 응답 없이 무한 대기

## 5. 적용한 수정
- 당시 수정은 `FSession`에 단순 bool 대신 send 상태 비트를 두는 것이었고, 현재 구조 분리 후에는 같은 구현이 [FIocpSession](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FIocpSession.h)로 이동했다.
  - `kSendInFlightFlag`
  - `kSendPendingFlag`
- enqueue 시 `kSendPendingFlag`를 세운다.
- `PostSend()`가 빈 queue로 끝나려 할 때 pending 비트를 확인하고 바로 다시 시도한다.
- send completion 이후에도 pending 비트 또는 잔여 queue가 있으면 다시 `PostSend()`를 건다.

대상 파일:
- [FIocpSession.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FIocpSession.h)
- [FIocpSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FIocpSession.cpp)
- [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.cpp)

## 6. 결과
- 같은 무timeout 1분 런을 다시 돌렸을 때 클라이언트가 정상 종료했다.
- 서버도 마지막에
  - `sessions=0`
  - `queuedSendBuffers=0`
  상태로 정리됐다.
- 이전에 보였던 `sessions=1`, `queuedSendBuffers=1` hang은 이 수정 후 짧은 재현에서 다시 나오지 않았다.

## 7. 결론
- 이번 건은 `응답이 느려서 timeout` 난 문제가 아니라, 레거시에서 중요하게 유지하던 send 재기동 보장이 빠진 채 옮겨오면서 생긴 lost-wakeup 계열 버그였다.
- 따라서 이후 `room-flow timeout`을 다시 볼 때는, 이 send fix 적용 이후에도 남는 실패만 별도로 평가해야 한다.
