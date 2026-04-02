# ContentsRuntime 트러블슈팅

## 1. bootstrap 단계 `chat-bootstrap` recv timeout

### 증상
- `EchoClient`가 `chat-bootstrap` 단계에서 `recv failed ... error=10060 (timeout)`으로 멈췄다.
- 서버 로그에는 같은 세션에 대해
  - `login succeeded`
  - `echo content enter`
  - `auth content leave`
  까지는 보이는데, 이어서 와야 할 snapshot 처리 로그가 없었다.

### 최초 재현
- 2026-04-03 기준 `Run-ContentsRuntimeRaceValidation.ps1`의 race injection 검증에서 실제로 한 번 재현했다.
- 당시 클라이언트 오류:
  - `session[10] failed: recv failed at stage=chat-bootstrap sessionIndex=10 error=10060 (timeout)`

## 2. 디버깅 추적 과정

### 2.1 처음 확인한 사실
- 실패 세션은 로그인까지는 정상 진행됐다.
- 즉 `login-response` 단계는 통과했고, 문제는 그 다음 bootstrap 단계였다.
- 이것만으로도 `LoginRq` 자체가 유실된 문제는 아니라는 점을 먼저 확인할 수 있었다.

### 2.2 클라이언트 계측 추가
- `EchoClient`에 `--recv-timeout-ms`를 추가해서 무기한 대기 대신 어느 단계에서 막히는지 로그로 남기게 했다.
- bootstrap 단계는 다음 순서로 나눠 확인했다.
  1. `login-response`
  2. `chat-bootstrap`
  3. `echo-response`
- 그 결과 실제 실패 지점이 `chat-bootstrap`임을 확정했다.

### 2.3 서버 send 실패 가능성 확인
- `FAuthContent`에서 `LoginRp` send 실패를 로그로 남기도록 했다.
- `FEchoContent`에서 `RoomSnapshotRp`, `RoomBinarySnapshotNoti` send 실패도 로그로 남기도록 했다.
- 재현 로그에는 이 send 실패 로그가 없었다.
- 따라서 단순 send API 실패가 1차 원인일 가능성은 낮다고 봤다.

### 2.4 bootstrap trace 추가
- 전체 패킷 로그는 타이밍을 크게 흔들 수 있어서, bootstrap 관련 패킷만 찍는 trace를 넣었다.
- 확인한 지점:
  - 서버 ingress에서 `LoginRq`, `RoomSnapshotRq`가 들어왔는지
  - `EchoContent::HandleRoomSnapshotRq`까지 실제 도달했는지
  - snapshot 응답 두 개가 전송됐는지
  - 클라이언트가 login 응답, snapshot 요청, snapshot 응답, binary snapshot을 각각 받았는지

### 2.5 잘못된 가설 배제
- `lock-free packet inbox` 자체 유실을 처음엔 의심했다.
- 하지만 bootstrap trace 기준으로는 `RoomSnapshotRq`가 서버 ingress까지는 보이는 경우가 있었고, lock-free queue만 단독 원인이라고 단정할 수 없었다.
- 또 전체 패킷 로그를 켜면 재현이 줄어들어, 단순 queue corruption보다 타이밍 race 가능성이 더 커 보였다.

### 2.6 결정적 코드 점검
- `FAuthContent`의 로그인 성공 처리 순서를 다시 확인했다.
- 기존 순서:
  1. `LoginRp`를 비동기로 전송 enqueue
  2. 그 다음 `MoveSession(sessionId, kEchoContentId)`
- 이 순서에서는 클라이언트가 `LoginRp`를 먼저 받고 바로 `RoomSnapshotRq`를 보내는 순간,
  서버 세션 라우트가 아직 `AuthContent`를 가리킬 수 있다.

### 2.7 최종 원인 확정
- `RoomSnapshotRq`는 원래 `EchoContent`가 처리해야 한다.
- 하지만 전이 완료 전에 다음 콘텐츠 요청이 도착하면 그 요청이 아직 이전 콘텐츠로 라우팅될 수 있다.
- `AuthContent`는 `LoginRq`만 처리하고 나머지는 무시하므로,
  `RoomSnapshotRq`가 `AuthContent`로 들어간 순간 snapshot 요청은 조용히 버려진다.
- 그 결과 클라이언트는 `RoomSnapshotRp` / `RoomBinarySnapshotNoti`를 기다리다가 timeout 난다.

## 3. 원인
- 원인은 `FAuthContent`의 처리 순서 race였다.
- 기존 순서:
  1. `LoginRp`를 비동기로 전송 enqueue
  2. 그 다음 `MoveSession(sessionId, kEchoContentId)`
- 이 순서에서는 클라이언트가 `LoginRp`를 먼저 받고 곧바로 `RoomSnapshotRq`를 보내는 순간,
  서버 쪽 세션 라우트가 아직 `AuthContent`를 가리킬 수 있다.
- 그러면 `RoomSnapshotRq`가 `EchoContent`가 아니라 `AuthContent`로 들어가고,
  `AuthContent`는 `LoginRq`만 처리하므로 snapshot 요청이 조용히 버려진다.
- 결과적으로 클라이언트는 `RoomSnapshotRp` / `RoomBinarySnapshotNoti`를 기다리다가 timeout 난다.

## 4. 수정
- `FAuthContent.cpp`에서 성공 로그인 시 순서를 다음처럼 변경했다.
  1. `MoveSession(sessionId, kEchoContentId)`
  2. `LoginRp` 전송
- 즉 클라이언트가 login 응답을 받은 시점에는 이미 서버 라우트가 `EchoContent`로 넘어가 있게 만들었다.

## 5. 검증
- 수정 후 `EchoServer` 단독 재빌드 성공
- `Run-ContentsRuntimeRaceValidation.ps1`
  - `DurationSeconds=10`
  - `Sessions=20`
  - `RacePeriod=1`
  - `RaceMode=sleep0`
  조건 스모크 통과
- 추가로 `100 sessions`, `RacePeriod=1` 단기 반복 검증에서도 동일 증상은 다시 나오지 않았다.

## 6. 비고
- 이전 2시간 장시간 테스트는 `EchoClient` 종료 버그 때문에 신뢰 가능한 합격 결과로 볼 수 없다.
- bootstrap race 재현과 원인 분석은 장시간 soak 결과와 분리해서 해석해야 한다.

## 7. 공격적 재접속 시 `connect failed: 10055`

### 증상
- `300 sessions + reconnectProbabilityPercent=25` 같은 공격적 검증에서
  `session[229] failed: connect failed: 10055`
  가 발생했다.

### 해석
- 이건 bootstrap timeout 원인과는 별개다.
- `10055`는 소켓/버퍼 자원 부족 계열 오류라서, 매우 공격적인 재접속 부하에서 발생한 OS 자원 한계로 봐야 한다.
- 따라서 `chat-bootstrap timeout` 원인 분석 근거로 사용하면 안 된다.
