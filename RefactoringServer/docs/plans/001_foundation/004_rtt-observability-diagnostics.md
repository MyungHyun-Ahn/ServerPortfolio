# RTT Observability Diagnostics Plan

## 1. 목적
- `EchoClient`에서 시작한 `Rq -> Rp` 왕복 시간(RTT) 계측을 특정 실행기 전용 코드가 아니라 공용 진단 모듈로 승격한다.
- 이후 동일한 계측 자료구조를 `EchoClient`, 장시간 테스트 실행기, 서버 측 진단 코드가 함께 재사용할 수 있게 만든다.
- 현재 timeout 분석의 핵심 질문인 `응답 유실인가, 단순 지연인가`를 공용 계측 모듈로 검증 가능하게 한다.

## 2. 왜 `NetworkLib`가 아니라 `Foundation/Diagnostics`인가
- 현재 측정하려는 대상은 소켓 전송 자체의 RTT가 아니라 `RoomListRq -> RoomListRp`, `EchoRq -> EchoRp` 같은 애플리케이션 요청/응답 지연이다.
- 이 계측은 `NetworkLib` 전용 정책이 아니라 여러 프로젝트가 함께 쓸 수 있는 공용 진단 기능에 가깝다.
- `Foundation`은 `NetworkLib`보다 아래 계층이므로, 공용 계측 타입을 여기 두면 `EchoClient`, `EchoServer`, `ContentsRuntime`이 같은 도구를 공유하기 쉽다.
- 반대로 `Foundation/Diagnostics`가 `NetworkLib::Containers`에 직접 의존하면 계층이 꼬이므로, 진단 모듈은 `NetworkLib`에 역의존하지 않는 방향으로 설계해야 한다.

## 3. 목표 범위

### 3-1. 1차 목표
- 스레드별 `thread_local` 누적 버퍼
- 1분 단위 snapshot 집계
- `avg` 두 종류 기록
  - `minute_avg_ms`
  - `overall_avg_ms`
- `top 3 max RTT` 기록
- `timeout_count` 기록
- CSV sink 제공
- 클라이언트가 `stage` 이름과 `sessionIndex`를 붙여서 기록 가능

### 3-2. 1차 제외 범위
- Prometheus, ETW, 외부 telemetry 시스템 연동
- JSON sink
- p95/p99 percentile
- 그래프 시각화
- 서버/클라이언트 간 공통 request id 체계까지 한 번에 도입

## 4. 요구사항 정리
- hot path에서 전역 락을 잡지 않는다.
- 계측 때문에 기존 timeout 재현 실험을 오염시키지 않도록 한다.
- outlier 분석이 가능해야 하므로 `max top3`에는 RTT 값뿐 아니라 시각과 세션 정보가 남아야 한다.
- `min`은 1차 범위에서 제외한다.
- timeout이 발생해도 그 stage의 timeout count가 집계에 남아야 한다.

## 5. 제안 구조

### 5-1. 디렉터리
- `RefactoringServer/Foundation/Diagnostics/Rtt/`

### 5-2. 공용 타입
- `ERttSampleKind`
  - `RequestResponse`
  - 이후 필요 시 `ProcessingLatency`, `SendLatency` 같은 확장 가능
- `SRttTopSample`
  - `rttMs`
  - `sessionIndex`
  - `sentEpochMs`
  - `recvEpochMs`
- `SRttStageAggregate`
  - `sampleCount`
  - `timeoutCount`
  - `totalRttMs`
  - `topSamples[3]`
- `SRttSnapshot`
  - `bucketStartEpochSeconds`
  - stage별 aggregate 묶음

### 5-3. 런타임 구성
- `FRttThreadLocalCollector`
  - 각 스레드의 `thread_local` 누적 버퍼 담당
  - request sent 시각과 response received 시각 차이를 누적
  - 1분 bucket 경계가 바뀌면 snapshot 생성
- `FRttSnapshotQueue`
  - collector가 만든 snapshot을 logger thread로 전달
  - 계층 규칙상 `NetworkLib::Containers::FLockFreeQueue`를 그대로 쓰지 않는다.
  - 선택지:
    - `Foundation/Diagnostics` 자체 queue 구현
    - 아주 가벼운 mutex queue
    - 이후 generic queue를 `Foundation`으로 승격
- `FRttAggregator`
  - minute / overall 집계
  - top3 merge
- `IRttSink`
  - snapshot 또는 minute aggregate를 외부로 기록
- `FCsvRttSink`
  - 1분 단위 CSV 기록

## 6. queue 경계 원칙
- 이번 작업의 핵심은 `hot path 무락`이지, queue 구현을 무조건 lock-free로 만드는 것 자체가 아니다.
- `thread_local -> snapshot`까지는 무락으로 간다.
- snapshot 전달 queue는 `Foundation` 계층 규칙을 깨지 않는 범위에서 선택한다.
- 즉:
  - `Foundation`이 `NetworkLib`에 의존하는 형태는 허용하지 않는다.
  - lock-free queue가 꼭 필요하면 generic queue를 별도로 `Foundation`으로 승격하는 후속 작업으로 분리한다.
- 1차 구현에서는 `snapshot 전송 빈도`가 분당 1회 수준이므로, queue 구현보다 계층과 계측 정확성 보장이 우선이다.

## 7. EchoClient 적용 방식
- `EchoClient`는 공용 모듈에 stage 이름과 `sessionIndex`만 넘긴다.
- stage 예시:
  - `login-response`
  - `room-list`
  - `room-enter`
  - `room-change-list`
  - `room-change`
  - `echo-response`
- 각 `Rq` 전송 시 `BeginRequest(...)`
- 각 `Rp` 수신 시 `RecordSample(...)`
- timeout 발생 시 `RecordTimeout(...)`
- CSV 컬럼은 최소 아래를 포함한다.
  - `bucket_start_local`
  - `stage`
  - `minute_count`
  - `minute_timeout_count`
  - `minute_avg_ms`
  - `overall_count`
  - `overall_timeout_count`
  - `overall_avg_ms`
  - `minute_max1~3`
  - `overall_max1~3`

## 8. 서버 재사용 방향
- 1차 구현은 클라이언트 계측이 우선이다.
- 이후 서버에서는 같은 자료구조를 다음 용도로 재사용한다.
  - `packet ingress -> handler complete`
  - `handler begin -> response send`
  - `content transition begin -> transition completion`
- 단, 서버는 stage naming과 request/response 대응 정책이 클라이언트와 다를 수 있으므로 sink와 태그 규칙은 별도 설정값으로 둔다.

## 9. 수명주기 책임
- logger thread는 `Foundation`이 자동으로 전역 생성하지 않는다.
- `Foundation/Diagnostics`는 다음 두 가지를 제공한다.
  - 공용 `FRttMetricsRuntime`
  - `Start() / Stop()` 가능한 `FRttCsvLogger`
- 실제 수명주기 소유자는 소비 프로젝트다.
  - `EchoClient main`
  - 이후 `EchoServer`, 테스트 런처 등
- 즉 실행기는 자기 시작 시점에 logger를 만들고, worker 종료 후 명시적으로 `Stop()`을 호출해 마지막 bucket까지 flush한다.

## 10. 단계별 작업 순서
1. 현재 `EchoClient/Main.cpp`에 흩어진 RTT 자료구조를 식별한다.
2. 공용으로 승격 가능한 타입과 클라이언트 전용 정책을 분리한다.
3. `Foundation/Diagnostics/Rtt`에 공용 타입과 collector/aggregator/sink 인터페이스를 만든다.
4. `EchoClient`는 공용 모듈 호출만 하도록 정리한다.
5. CSV sink를 붙이고 단기 런으로 파일 형식과 값이 맞는지 검증한다.
6. timeout을 제거한 장시간 런에서 `5000ms` 초과 RTT가 실제 존재하는지 확인한다.

## 11. 검증 계획

### 11-1. 기능 검증
- `EchoClient` 단기 실행에서 CSV 파일 생성 확인
- 1분 bucket row 생성 확인
- `minute_avg_ms`, `overall_avg_ms`, `top3` 값이 비어 있지 않게 기록되는지 확인

### 11-2. 회귀 검증
- 기존 `EchoClient` 스모크가 유지되는지 확인
- RTT 계측을 켜지 않았을 때 기존 동작과 결과가 바뀌지 않는지 확인

### 11-3. 목적 검증
- timeout을 끈 장시간 런에서 stage별 최대 RTT를 확인
- 기존 timeout 값(`5000ms`)보다 긴 RTT가 실제 있었는지 확인
- 늦게라도 응답이 왔다면 timeout 실패를 로직 유실과 분리해서 해석할 수 있게 한다.

## 12. 현재 결론
- RTT 자료구조의 공용화는 필요하다.
- 위치는 `NetworkLib`보다 `Foundation/Diagnostics`가 더 적합하다.
- 핵심 원칙은 `hot path 무락`, `Foundation -> NetworkLib 역의존 금지`, `클라이언트/서버 공용 재사용 가능 구조`다.
