# ContentsRuntime 트러블슈팅

## 1. 목적

이 문서는 `ContentsRuntime`와 `EchoServer/EchoClient`를 붙여서 검증하는 과정에서 발견한 문제를 다음 기준으로 정리한다.

- 어떤 증상이 있었는가
- 어떤 조건에서 재현되었는가
- 원인을 어떻게 좁혀 갔는가
- 무엇이 확정되었고, 무엇이 아직 가설인가
- 어떤 수정/실험을 했고 결과가 어땠는가

현재까지 정리 대상은 다음 두 가지다.

1. 초기 bootstrap 전이 race
2. `Lobby -> Room -> RoomChange -> RoomEcho` 흐름에서 드물게 발생하는 `echo-response timeout`

## 2. bootstrap 전이 race

### 2.1 증상

- 클라이언트가 `chat-bootstrap` 단계에서 멈추고 `10060 timeout` 발생
- 서버 로그에는 `login succeeded`, `echo content enter`, `auth content leave`까지만 있고, 이후 snapshot 응답 로그가 없음

### 2.2 재현 조건

- `Run-ContentsRuntimeRaceValidation.ps1`
- `RaceMode=sleep0`
- `RacePeriod=1`
- `100 sessions`

### 2.3 원인 추적 과정

1. 클라이언트 recv timeout 계측을 추가해 어느 단계에서 멈추는지 확인했다.
   - 결과: `chat-bootstrap`
2. 서버 send 실패 가능성을 먼저 점검했다.
   - `LoginRp`, `RoomSnapshotRp`, `RoomBinarySnapshotNoti` send 실패 로그는 보이지 않았다.
3. bootstrap trace를 넣어서 `LoginRp` 수신 후 `RoomSnapshotRq`가 언제 들어오는지 확인했다.
4. 최종적으로 `FAuthContent`의 처리 순서를 확인했다.
   - 기존 순서:
     1. `LoginRp` 전송 enqueue
     2. `MoveSession`
   - 이 경우 클라이언트가 `LoginRp`를 받자마자 `RoomSnapshotRq`를 보내면, 서버 라우팅이 아직 `AuthContent`를 가리키는 순간이 생겼다.

### 2.4 확정 원인

- `다음 콘텐츠 요청을 허용하는 Rp`보다 `MoveSession`이 늦게 실행되면서 생긴 전이 race

### 2.5 적용한 해결

- 순서를 다음처럼 변경했다.
  1. `MoveSession`
  2. `LoginRp`

### 2.6 결과

- bootstrap timeout은 사라졌다.
- 이 경험을 바탕으로 별도 문서 [003_content-transition-rules.md](/d:/Project/ServerPortfolio/RefactoringServer/docs/reviews/002_contentsruntime/003_content-transition-rules.md)에 콘텐츠 전이 규칙을 정리했다.

## 3. Room 흐름 timeout

### 3.1 최초 증상

- 장시간 또는 반복 room-change 중 특정 세션이 `echo-response` 단계에서 `10060 timeout`
- 대표 로그:
  - [client_20260403_185203.err.log](/d:/Project/ServerPortfolio/RefactoringServer/Out/roomflow_longrun/client_20260403_185203.err.log)
  - `session[0] failed: recv failed at stage=echo-response sessionIndex=0 error=10060 (timeout)`
- 서버는 살아 있지만 트래픽이 끝난 뒤 idle 상태로 남아 있는 경우가 있었다.

### 3.2 최초 재현 조건

- 서버
  - `--headless --room-count 50 --room-capacity 4 --contents-fail-fast`
- 클라이언트
  - `--sessions 100`
  - `--count 2`
  - `--hold-seconds 7200`
  - `--interval-ms 200`
  - `--recv-timeout-ms 5000`
  - `--room-change-probability-percent 70`
  - `--max-room-enter-retries 20`
  - `--max-room-change-retries 5`

## 4. 원인 추적 과정

### 4.1 짧은 재현 조건으로 압축

문제를 더 빨리 드러내기 위해 다음과 같은 공격적 조건으로 반복 재현했다.

- 서버
  - `--headless --room-count 50 --room-capacity 4 --contents-fail-fast`
- 클라이언트
  - `--sessions 20~100`
  - `--count 2`
  - `--payload-size 9 또는 16`
  - `--hold-seconds 300`
  - `--interval-ms 0`
  - `--recv-timeout-ms 5000`
  - `--room-change-probability-percent 100`
  - `--max-room-enter-retries 20`
  - `--max-room-change-retries 5~10`

### 4.2 실패 세션 trace 추가

- 클라이언트에 `--trace-session-index`
- 서버에 bootstrap trace를 넣어서 특정 세션의 흐름을 따라갔다.

짧은 단일 세션 trace에서 확인된 사실:

- `RoomChangeRp`를 받기 전에 클라이언트가 `EchoRq`를 보내지는 않는다.
- 정상 세션에서는 아래 흐름이 모두 보였다.
  - 서버 ingress
  - `FContentRuntime::EnqueuePacket` accepted / posted
  - thread dequeue
  - room `OnPacket`
  - `echo request accepted`
  - `echo response sent`

즉 단일 세션 짧은 재현으로는 문제가 드러나지 않았다.

### 4.3 all-session trace로 전환

특정 세션 하나를 미리 찍는 방식으로는 실패 세션을 놓치기 쉬워서, 이후에는 다음 방식으로 바꿨다.

- `--bootstrap-trace`만 켜고 `--trace-user-id`는 지정하지 않음
- `tracedSessionId == nullptr`일 때 모든 세션 trace를 남기도록 변경

목적:

- 실패가 난 뒤 `client.err.log`에서 `sessionIndex`를 먼저 찾고
- `userId = 1000 + sessionIndex`
- 서버 `login succeeded` 로그에서 대응하는 `sessionId`를 찾은 다음
- 그 `sessionId` 기준으로 서버 흐름을 역추적하기 위함

### 4.4 generation / local state 가설 점검

의심했던 가설:

- room content 내부의 `m_sessionGenerations`가 runtime route table보다 늦게 갱신되어 stale packet으로 버리는 것

시도:

- 브리지 기준 authoritative route / instance 조회 추가
- local generation mismatch를 보정하는 완화 실험

결과:

- 보조적인 가설로는 유효했지만, 이것만으로 문제를 설명하거나 해결하지는 못했다.

### 4.5 OnEnter 완료 후 completion callback 방식 적용

사용자 제안 전 실험했던 방향:

- `MoveSessionToInstanceWithCompletion(...)`
- target content thread의 `OnEnter` 완료 후 success `Rp`를 보내도록 변경

적용 대상:

- `RoomEnterRp success`
- `RoomChangeRp success`

의도:

- bootstrap 버그와 같은 계열이라면 `Rp`를 너무 빨리 보내서 생기는 문제일 수 있으므로, 실제 `OnEnter`가 끝난 뒤에만 success `Rp`를 보내면 해결될 수 있다고 판단했다.

결과:

- 문제는 완전히 사라지지 않았다.
- 즉 `RoomChangeRp`를 `OnEnter` 이후로 늦추는 것만으로는 충분하지 않았다.

### 4.6 최신 상태에서 확인된 사실

현재까지 확인된 사실은 다음과 같다.

- 클라이언트는 `RoomChangeRp success`를 받은 뒤에만 `EchoRq`를 보낸다.
- 짧은 단일 세션 deep trace에서는 다음 네 지점 모두 정상이다.
  1. 두 번째 `EchoRq`가 서버 ingress까지 도달
  2. `EnqueuePacket` 전후에서 사라지지 않음
  3. target room `OnPacket`까지 도달
  4. `EchoRp` send도 성공
- 따라서 문제는 단순한 단일 세션 타이밍 버그라기보다, 다중 세션 / 장시간 / 특정 상태 조합에서만 드물게 드러나는 race일 가능성이 높다.

## 5. 최근 재현 실험

### 5.1 3분 / 200세션 / all-session trace

설정:

- 서버
  - `--headless --room-count 100 --room-capacity 4 --contents-fail-fast --bootstrap-trace`
- 클라이언트
  - `--sessions 200 --count 2 --payload-size 16 --hold-seconds 180 --interval-ms 0 --packets-per-send 2 --recv-timeout-ms 5000 --room-change-probability-percent 100 --max-room-enter-retries 20 --max-room-change-retries 10 --bootstrap-trace --quiet`

결과:

- [server_20260404_005105.log](/d:/Project/ServerPortfolio/RefactoringServer/Out/roomflow_short_repro/server_20260404_005105.log)
- [client_20260404_005105.err.log](/d:/Project/ServerPortfolio/RefactoringServer/Out/roomflow_short_repro/client_20260404_005105.err.log)
- 성공

### 5.2 3분 / 250세션 / room contention 강화

설정:

- 서버
  - `--room-count 20 --room-capacity 3`
- 클라이언트
  - `--sessions 250 --count 1 --interval-ms 0 --room-change-probability-percent 100`

결과:

- [client_20260404_005542_contention.err.log](/d:/Project/ServerPortfolio/RefactoringServer/Out/roomflow_short_repro/client_20260404_005542_contention.err.log)
- 실패는 났지만 우리가 찾는 버그가 아니라 정상 실패였다.
  - `session[4] failed: no joinable room available.`

해석:

- room contention을 너무 강하게 주면 rare race보다 `방 없음` 정상 실패가 먼저 튀어나온다.

### 5.3 3분 / 250세션 / balanced 설정

설정:

- 서버
  - `--room-count 80 --room-capacity 4`
- 클라이언트
  - `--sessions 250 --count 1 --interval-ms 0 --room-change-probability-percent 100`

결과:

- [server_20260404_005919_balanced.log](/d:/Project/ServerPortfolio/RefactoringServer/Out/roomflow_short_repro/server_20260404_005919_balanced.log)
- [client_20260404_005919_balanced.err.log](/d:/Project/ServerPortfolio/RefactoringServer/Out/roomflow_short_repro/client_20260404_005919_balanced.err.log)
- 성공

## 6. 분석용 race injection 실험

문제가 희귀하다고 판단해, 장시간만 기다리지 않고 전이 경계에 race window를 인위적으로 넓히는 실험을 추가했다.

### 6.1 전이 응답 직전 injection

추가 옵션:

- `--transition-race-injection`
- `--transition-race-mode sleep0`

적용 위치:

- `RoomEnterRp success` 전송 직전
- `RoomChangeRp success` 전송 직전

결과:

- [server_20260404_010614.log](/d:/Project/ServerPortfolio/RefactoringServer/Out/roomflow_transition_race/server_20260404_010614.log)
- [client_20260404_010614.err.log](/d:/Project/ServerPortfolio/RefactoringServer/Out/roomflow_transition_race/client_20260404_010614.err.log)
- 3분 / 250세션 기준 성공

### 6.2 전이 응답 직전 + RoomChangeRp 직후 + 첫 EchoRq 직전 injection

추가 옵션:

- `--transition-race-injection --transition-race-mode sleep0`
- `--post-room-change-race-injection --post-room-change-race-mode sleep0`
- `--first-echo-race-injection --first-echo-race-mode sleep0`

의도:

- `OnEnter 완료 -> RoomChangeRp`
- `RoomChangeRp -> 첫 EchoRq`
- target room에서 전이 직후 첫 `EchoRq` 처리

세 경계를 모두 벌려서 재현률을 높여 보려는 실험

결과 1:

- [server_20260404_011134_double.log](/d:/Project/ServerPortfolio/RefactoringServer/Out/roomflow_transition_race/server_20260404_011134_double.log)
- [client_20260404_011134_double.err.log](/d:/Project/ServerPortfolio/RefactoringServer/Out/roomflow_transition_race/client_20260404_011134_double.err.log)
- 성공

결과 2 (`room-change=90`)

- [server_20260404_011548_rc90.log](/d:/Project/ServerPortfolio/RefactoringServer/Out/roomflow_transition_race/server_20260404_011548_rc90.log)
- [client_20260404_011548_rc90.err.log](/d:/Project/ServerPortfolio/RefactoringServer/Out/roomflow_transition_race/client_20260404_011548_rc90.err.log)
- 성공

해석:

- 현재까지는 위 세 지점을 벌려도 3분 / 250세션 기준으로는 재현률이 충분히 올라가지 않았다.
- 즉 문제는 단순 전이 경계 타이밍만으로 설명되지 않거나, 더 긴 누적 시간 / 더 많은 상태 조합이 필요할 수 있다.

## 7. 현재까지 확정된 것

- bootstrap 버그는 원인과 해결이 확정되었다.
- room-flow 문제는 `RoomChangeRp를 너무 빨리 보내는 단일 원인`만으로는 더 이상 설명되지 않는다.
- 클라이언트가 `RoomChangeRp` 전에 `EchoRq`를 보내는 구조는 아니다.
- 짧은 단일 세션 trace에서는
  - ingress
  - enqueue
  - thread dequeue
  - room `OnPacket`
  - `EchoRp send`
  까지 모두 정상이다.

## 8. 현재까지 확정되지 않은 것

아직 확정되지 않은 핵심 질문은 다음과 같다.

- 장시간 / 다중 세션에서 실패한 바로 그 세션의 마지막 `EchoRq`가 서버 ingress까지 들어왔는가
- 들어왔다면 `EnqueuePacket` 전후에서 사라졌는가
- target room `OnPacket`까지 왔는데 generation / instance check에서 버려졌는가
- `EchoRp`를 만들었지만 send가 실패했는가

즉, 현재 남은 과제는 **실패 세션 하나를 all-session trace에서 정확히 특정하고, 그 세션의 마지막 실패 구간만 끝까지 추적해 네 지점 중 어디서 끊기는지 확정하는 것**이다.

## 9. 현재 결론

- 문제는 여전히 미해결이다.
- 다만 지금까지의 실험으로 다음 범위까지는 좁혀졌다.
  - 단순 bootstrap race는 아님
  - 클라이언트가 `RoomChangeRp` 전에 `EchoRq`를 보내는 문제는 아님
  - 짧은 단일 세션 흐름 자체는 정상
  - 희귀한 다중 세션 / 장시간 / 상태 조합 race 가능성이 높음

다음 분석 단계는:

1. all-session trace를 유지한 상태에서 실패 런 확보
2. `client.err.log`에서 실패 `sessionIndex` 추출
3. 대응 `userId`, `sessionId`를 찾아 서버 로그에서 같은 세션의 마지막 `EchoRq -> EchoRp` 흐름만 역추적

이 단계에서 원인이 확정되면, 그 다음 해결 방법은 사용자와 합의 후 진행한다.

## 10. send-post lost-wakeup 추가 확인

### 10.1 no-timeout 실험에서 보인 새로운 패턴

- `recv timeout`을 모두 끄고 1분 런을 돌렸는데도 클라이언트가 끝나지 않고 멈췄다.
- 외부 watchdog이 `90초` 뒤 강제 종료했다.
- 이때 서버는 오랫동안 다음 상태를 유지했다.
  - `sessions=1`
  - `recvTPS=0`
  - `sendTPS=0`
  - `queuedSendBuffers=1`
  - `totalWSASendCalls` 증가 없음

해석:

- 이 패턴은 `응답이 매우 느리다`보다는 `send queue에 버퍼가 남아 있는데 WSASend가 다시 시작되지 않는다`에 더 가깝다.

### 10.2 레거시와 비교해서 좁혀진 원인

- 레거시 프로젝트는 `SendPacket()` / `EnqueuePacket()` 분리보다 더 중요한 보장이 하나 있었다.
- `m_iSendFlag + ENQUEUE_FLAG`로 `send 중 새 enqueue가 들어오면 다음 PostSend를 놓치지 않게` 만들고 있었다.
- 현재 `RefactoringServer`는 이 부분이 `std::atomic<bool> m_sendInFlight`로 단순화돼 있었다.

즉 이번 건은:

- `EnqueuePacket` 같은 특수 API를 잘못 써서 `PostSend`를 안 불렀다

가 아니라

- 레거시의 send 재기동 보장이 빠진 상태에서 lost-wakeup race가 생겼다

로 보는 게 맞다.

### 10.3 현재 코드에서 가능했던 race

1. send completion 쪽이 `PostSend()`에 들어가 `m_sendInFlight=true`를 잡는다.
2. queue를 확인했더니 비어 있어서 종료하려고 한다.
3. 그 사이 다른 스레드가 새 send buffer를 enqueue하고 `PostSend()`를 호출한다.
4. 하지만 `m_sendInFlight=true`라서 두 번째 `PostSend()`는 바로 빠진다.
5. 첫 번째 `PostSend()`는 `m_sendInFlight=false`로 내리고 끝난다.
6. 결과적으로 queue에는 버퍼가 남았는데 send는 재시작되지 않는다.

이 패턴은 실제 관찰된

- `queuedSendBuffers=1`
- `sendTPS=0`
- `WSASend` 호출 정지

와 잘 맞는다.

### 10.4 적용한 수정과 결과

- `FSession`에 send 상태 비트를 도입했다.
  - `kSendInFlightFlag`
  - `kSendPendingFlag`
- enqueue 시 pending 비트를 세운다.
- `PostSend()`가 빈 queue로 끝나려 할 때 pending 비트를 보고 다시 돈다.
- send completion 뒤에도 pending 비트 또는 잔여 queue가 있으면 다시 `PostSend()`를 건다.

수정 파일:

- [FSession.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FSession.h)
- [FSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FSession.cpp)
- [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.cpp)

짧은 무timeout 재실행 결과:

- 클라이언트 정상 종료
- 서버 마지막 상태 `sessions=0`, `queuedSendBuffers=0`
- 이전처럼 `sessions=1`, `queuedSendBuffers=1`로 멈추는 패턴은 다시 나오지 않았다.

### 10.5 현재 해석

- room-flow에서 보였던 일부 hang/timeout은 콘텐츠 전이 로직이 아니라 `NetworkLib` send 재기동 보장 누락으로 설명된다.
- 따라서 이후 `echo-response timeout`, `room-change-list timeout`, `room-change timeout`은 이 send fix 적용 이후 기준으로 다시 분리해서 평가해야 한다.

## 11. 무timeout 6시간 RTT 확인

### 11.1 목적

- send fix 이후에도 실제 응답 유실이 남아 있는지, 아니면 기존 `10060 timeout`이 단순 지연인지 확인하기 위해 `recv timeout`을 모두 끄고 장시간 런을 돌렸다.

### 11.2 조건

- 서버
  - `--headless --room-count 80 --room-capacity 4 --contents-fail-fast`
- 클라이언트
  - `--sessions 250`
  - `--count 1`
  - `--hold-seconds 21600`
  - `--interval-ms 0`
  - `--packets-per-send 1`
  - `--recv-timeout-ms 0`
  - `--room-list-recv-timeout-ms 0`
  - `--echo-recv-timeout-ms 0`
  - `--room-change-probability-percent 90`
  - `--max-room-enter-retries 20`
  - `--max-room-change-retries 10`
  - `--rtt-csv-path ...`
  - `--rtt-flush-interval-seconds 60`

### 11.3 결과

- 클라이언트는 `echo validation succeeded.`로 정상 종료했다.
- `client.err.log`는 비어 있었다.
- RTT CSV는 6시간 전체 구간을 끝까지 기록했다.
- `timeout_count`는 모든 stage에서 `0`이었다.

### 11.4 중요한 관찰

- `room-change` RTT는 실제로 `5000ms`를 넘긴 값이 기록됐다.
  - `5006.861ms`
- `room-change`의 다음 상위 값도 `4991.590ms`였다.
- `echo-response`, `room-change-list`도 `3900ms`대 outlier가 있었다.

즉:

- 이전 `10060 timeout`은 반드시 `응답 유실`을 뜻하지 않는다.
- 적어도 일부는 `응답은 결국 왔지만, 기존 5000ms timeout보다 늦게 왔던 경우`로 해석할 수 있다.

### 11.5 최종 해석

- 이번 이슈에서 확정된 로직 버그는 `send-post lost-wakeup`이었다.
- 그 수정 이후 무timeout 장시간 런이 정상 종료했고, 5초를 넘는 정상 RTT도 관측됐다.
- 따라서 현재 남아 있는 `10060 timeout`은 로컬 부하 환경에서의 지연 가능성이 높다고 보는 것이 맞다.
