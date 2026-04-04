# Room-Flow Timeout 요약

## 1. 범위
- 이 문서는 `Lobby -> Room -> RoomChange -> Echo` 흐름에서 드물게 발생하는 timeout 문제만 따로 정리한다.
- 자잘한 실험 로그나 시행착오는 빼고, 현재 시점에서 중요한 사실만 남긴다.
- 상세 로그와 긴 추적 과정은 [002_contents-runtime-troubleshooting.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\002_contents-runtime-troubleshooting.md)에 둔다.

## 2. 관찰된 증상
초기 관찰 기준으로는 timeout이 두 종류였다.

1. `echo-response timeout`
- 클라이언트가 `EchoRq`를 보낸 뒤 `EchoRp`를 기다리다가 `10060 timeout`으로 실패한다.
- 예:
  - `session[0] failed: recv failed at stage=echo-response ... error=10060 (timeout)`

2. `room-change-list timeout`
- 클라이언트가 `RoomChange` 이후 다음 `RoomListRq/Rp` 구간에서 `10060 timeout`으로 실패한다.
- 예:
  - `session[0] failed: recv failed at stage=room-change-list ... error=10060 (timeout)`

중요한 점:
- 두 증상은 같은 현상처럼 보일 수 있지만, 같은 원인으로 바로 묶지 않았다.
- 이후 분석에서는 `echo-response timeout`과 `room-change-list timeout`을 분리해서 봤다.

## 3. 최종 정리

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

### 3.2 send 쪽 실제 버그
- `recv timeout`을 모두 끈 1분 런에서 `queuedSendBuffers=1` 상태로 세션 하나가 멈추는 현상이 재현됐다.
- 이 문제는 콘텐츠 로직이 아니라 `NetworkLib` send 재기동 보장 누락 때문이었다.
- 원인과 수정은 [012_send-post-lost-wakeup-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\012_send-post-lost-wakeup-review.md)에 정리했다.

### 3.3 남은 timeout 해석
- send fix 이후 `recv timeout`을 모두 끄고 6시간 런을 다시 돌렸을 때 클라이언트는 정상 종료했다.
- 같은 런의 RTT CSV에는 기존 timeout 값 `5000ms`에 근접하거나 초과하는 정상 왕복이 실제로 기록됐다.
  - `room-change` overall max: `5006.861ms`
  - `room-change` overall max2: `4991.590ms`
  - `echo-response` overall max: `3907.221ms`
  - `room-change-list` overall max: `3896.309ms`
- 따라서 이전 `10060 timeout` 중 적어도 일부는 `응답 유실`이 아니라 `로컬 환경 부하로 인한 지연`으로 해석하는 것이 자연스럽다.
- 특히 서버/클라이언트를 같은 PC에서 함께 돌리고, 순간적으로 CPU 사용률이 크게 치솟는 환경에서는 `5000ms` timeout이 충분히 타이트할 수 있다.

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

### 4.3 `echo-response`만 더 잘 보이게 만든 재현 설정
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

### 4.4 최종 검증
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

결과:
- 클라이언트 정상 종료
- `timeout_count = 0`
- RTT CSV 6시간 전체 기록 완료

## 5. 시도한 것

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

## 6. 결론
- 이번 이슈에서 확인된 진짜 버그는 `NetworkLib` send 재기동 보장 누락이었다.
- 그 부분은 수정했고, 무timeout 6시간 런에서도 hang 없이 정상 종료했다.
- send fix 이후 기준으로 보면 예전 `10060 timeout` 중 상당수는 로직 유실보다 `지연 + 타이트한 timeout 설정`으로 설명하는 것이 더 타당하다.
- 따라서 현재 room-flow timeout 이슈는 다음 판단으로 종료할 수 있다.
  - `send lost-wakeup` 버그: 수정 완료
  - 남은 `10060 timeout`: 로컬 부하 환경에서의 지연 가능성 높음
  - `lock-free packet inbox`: 별도 장시간 검증과 이번 통합 검증 기준으로 유지 가능

## 7. 운영 메모
- 로컬 환경에서 서버/클라이언트를 함께 돌리는 검증이라면 `5000ms` timeout은 상황에 따라 너무 짧을 수 있다.
- 이후 자동 검증에서는
  - stage별 timeout을 더 여유 있게 두거나
  - RTT CSV 같은 진단 데이터를 함께 남기는 방식이 좋다.
