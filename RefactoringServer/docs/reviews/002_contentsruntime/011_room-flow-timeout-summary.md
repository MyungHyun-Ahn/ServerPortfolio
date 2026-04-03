# Room-Flow Timeout 요약

## 1. 범위
- 이 문서는 `Lobby -> Room -> RoomChange -> Echo` 흐름에서 드물게 발생하는 timeout 문제만 따로 정리한다.
- 자잘한 실험 로그나 시행착오는 빼고, 현재 시점에서 중요한 사실만 남긴다.
- 상세 로그와 긴 추적 과정은 [002_contents-runtime-troubleshooting.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\002_contents-runtime-troubleshooting.md)에 둔다.

## 2. 증상
현재 관찰된 timeout은 두 종류다.

1. `echo-response timeout`
- 클라이언트가 `EchoRq`를 보낸 뒤 `EchoRp`를 기다리다가 `10060 timeout`으로 실패한다.
- 예:
  - `session[0] failed: recv failed at stage=echo-response ... error=10060 (timeout)`

2. `room-change-list timeout`
- 클라이언트가 `RoomChange` 이후 다음 `RoomListRq/Rp` 구간에서 `10060 timeout`으로 실패한다.
- 예:
  - `session[0] failed: recv failed at stage=room-change-list ... error=10060 (timeout)`

중요한 점:
- 두 증상은 같은 현상처럼 보일 수 있지만, 현재는 같은 원인으로 단정하지 않는다.
- 지금은 `echo-response timeout`과 `room-change-list timeout`을 분리해서 본다.

## 3. 현재까지 좁혀진 원인

### 3.1 확정된 사실
- 클라이언트는 `RoomChangeRp success`를 받기 전에 다음 `EchoRq`를 보내지 않는다.
- 클라이언트는 `EchoRp`를 다 받기 전에 다음 `EchoRq`를 보내지 않는다.
- 단일 세션 짧은 재현에서는 다음 흐름이 모두 정상이다.
  - 서버 ingress
  - `FContentRuntime::EnqueuePacket`
  - target room thread dequeue
  - room `OnPacket`
  - `EchoRp` send
- 예전 bootstrap 계열 문제였던 `MoveSession 전에 Rp를 보내는 문제`는 이미 별도로 잡아서 해결했다.

### 3.2 아직 미확정인 부분
- 현재 room-flow timeout의 단일 root cause는 아직 확정되지 않았다.
- 특히 `echo-response timeout`은 단일 세션 짧은 재현에서는 안 나오고, 다중 세션/장시간 조건에서만 드물게 나타난다.
- 따라서 현재 가장 유력한 해석은 다음 둘 중 하나다.
  - `RoomChange -> 첫 Echo` 경계에서만 드물게 생기는 race
  - 장시간 누적 후 특정 세션만 늦게 처리되는 지연성 문제

### 3.3 `room-change-list timeout`에 대해 현재까지 확인된 것
- 실패 세션의 `RoomListRq`가 서버 ingress까지는 들어온 케이스를 확인했다.
- 즉 `RoomListRq` 자체가 아예 유실된 경우로는 설명되지 않는다.
- 다만 당시에는 `RoomListRp sent` 성공 로그가 없어서, 응답을 실제로 보냈는지 확정이 어려웠다.
- 이 때문에 최근에는 `room list response sent` 성공 로그를 추가했다.

## 4. 재현 방법

### 4.1 최초 장시간 재현
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

이 조건에서 `echo-response timeout`이 한 번 확인됐다.

### 4.2 짧은 공격적 재현
- 서버
  - `--headless --room-count 80 --room-capacity 4 --contents-fail-fast --bootstrap-trace`
- 클라이언트
  - `--sessions 250`
  - `--count 1`
  - `--hold-seconds 180`
  - `--interval-ms 0`
  - `--packets-per-send 1`
  - `--recv-timeout-ms 5000`
  - `--room-change-probability-percent 100`
  - `--max-room-enter-retries 20`
  - `--max-room-change-retries 10`
  - `--bootstrap-trace --quiet`

짧은 런에서는 종종 정상 종료된다. 즉 재현율이 낮다.

### 4.3 `echo-response`만 더 잘 보이게 만든 현재 재현 설정
- 목적:
  - `room-change-list timeout`이 먼저 터지는 걸 줄이고, `echo-response timeout`만 더 잘 보이게 만든다.
- 서버
  - `--headless --room-count 80 --room-capacity 4 --contents-fail-fast --bootstrap-trace`
- 클라이언트
  - `--sessions 250`
  - `--count 1`
  - `--hold-seconds 180` 또는 `1800`
  - `--interval-ms 0`
  - `--packets-per-send 1`
  - `--recv-timeout-ms 5000`
  - `--room-list-recv-timeout-ms 15000`
  - `--echo-recv-timeout-ms 5000`
  - `--room-change-probability-percent 100`
  - `--max-room-enter-retries 20`
  - `--max-room-change-retries 10`
  - `--bootstrap-trace --quiet`

핵심:
- `RoomList` 계열은 더 오래 기다린다.
- `Echo`만 기존처럼 짧게 timeout을 둔다.
- 그래서 실패가 나면 `echo-response`가 먼저 드러날 확률이 높다.

## 5. 지금까지 시도한 것

### 5.1 all-session trace
- 특정 세션 하나만 미리 찍는 방식 대신, 모든 세션 trace를 남기게 바꿨다.
- 이유:
  - 실패 세션이 랜덤하게 나오기 때문에, 먼저 실패 세션을 잡고 나서 서버 로그에서 역추적하는 편이 더 효율적이다.

### 5.2 서버 쪽 단계별 로그 보강
- 다음 지점 로그를 추가했다.
  - ingress
  - `EnqueuePacket`
  - target room `OnPacket`
  - `EchoRp` send
  - `RoomListRp` send success
- 목적:
  - 요청이 어디까지 왔는지,
  - 응답을 만들었는지,
  - 보냈는지를 단계별로 끊어서 보기 위함이다.

### 5.3 전이 완료 후 success Rp 보내기
- `MoveSessionToInstanceWithCompletion(...)`
- target room `OnEnter` 완료 뒤에 `RoomEnterRp success`, `RoomChangeRp success`를 보내도록 시도했다.
- 의도:
  - 전이 응답이 너무 빨라서 생기는 race인지 확인
- 결과:
  - bootstrap 계열 문제에는 맞는 해법이었지만, 현재 room-flow timeout을 완전히 없애지는 못했다.

### 5.4 race injection
- 다음 경계에 `Sleep(0)` / `SwitchToThread()` 기반 injection을 넣고 짧은 재현을 돌렸다.
  - `RoomEnter/RoomChange Rp` 직전
  - `RoomChangeRp` 직후
  - target room의 첫 `EchoRq` 처리 직전
- 목적:
  - rare race를 일부러 벌려서 재현율을 올리기 위함
- 결과:
  - 짧은 3분 런 기준으로는 결정적인 재현율 상승을 아직 못 만들었다.

### 5.5 단계별 timeout 분리
- `EchoClient`에 단계별 timeout 옵션을 추가했다.
  - `--room-list-recv-timeout-ms`
  - `--echo-recv-timeout-ms`
- 목적:
  - `room-list`가 먼저 죽어서 `echo-response` 분석이 흐려지는 문제를 줄이기 위함

## 6. 현재 결론
- 현재 room-flow timeout 전체가 아직 단일 원인으로 확정된 것은 아니다.
- 다만 `no-timeout` 실험으로 확인된 중요한 사실이 하나 있다.
  - 적어도 한 번의 장시간 hang은 `응답이 5초보다 늦게 온 것`이 아니라, 서버 send 경로의 재기동 보장 누락 때문에 `queuedSendBuffers=1` 상태로 멈춘 문제였다.
  - 이 부분은 별도 문서 [012_send-post-lost-wakeup-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\012_send-post-lost-wakeup-review.md)로 분리했다.
- 따라서 현재 판단은 다음과 같다.
  - 클라이언트가 요청 순서를 잘못 보내서 생기는 단순 문제는 아니다.
  - bootstrap 때와 같은 단순 전이 race 하나로는 설명되지 않는다.
  - 관찰된 일부 timeout/hang은 콘텐츠 로직이 아니라 send 재기동 보장 누락으로 설명된다.
  - 남은 실패 유형은 send fix 이후 기준으로 다시 분리해서 봐야 한다.

- 지금 가장 실용적인 분석 전략은 다음 둘이다.
  1. send fix 적용 후 같은 조건으로 다시 장시간 또는 반복 런을 돌린다.
  2. 그래도 실패 세션이 나오면 그 세션 기준으로
     - `RoomChangeRp success`
     - 첫 `EchoRq` ingress
     - `EchoRp sent`
     - disconnect
     순서를 서버 로그에서 다시 비교한다.

## 7. 다음에 확인할 것
- send fix 적용 후 `echo-response timeout` 실패가 다시 재현되는지 확인
- 같은 세션에서 서버가 `EchoRq`를 받았는데 `EchoRp sent`가 없는지 재검증
- `room-change-list timeout`은 send fix 이후에도 남는지 별도 실패 유형으로 분리 추적
