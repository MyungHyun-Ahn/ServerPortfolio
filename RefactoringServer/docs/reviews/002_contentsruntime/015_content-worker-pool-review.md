# Content Worker Pool Review

## 1. 요약
- 기존 `ContentsRuntime`는 `content instance = dedicated thread` 구조였다.
- 이번 변경으로 `FContentRuntime`가 worker pool을 소유하고, 여러 content instance를 소수의 worker thread 위에 배치하도록 바뀌었다.
- 현재 1차 배치 정책은 `round-robin` 고정 배치다.

## 2. 왜 바꿨나
- `Auth 1 + Lobby 1 + Room 80`이면 thread도 `82`개가 되는 구조였다.
- 이 구조는 레거시 `CContentsThread` 기반 pool 모델과 달랐고, `NetworkLib` 성능 비교 결과에도 content thread 수가 큰 잡음으로 들어갔다.
- 특히 10코어 머신에서 서버와 클라이언트를 같은 머신에 띄우면 room 수만큼 thread가 늘어나는 비용이 커졌다.

## 3. 현재 구조
### 3-1. runtime
- [FContentRuntime.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Routing\FContentRuntime.cpp)
  - `SContentSlot`은 이제 dedicated thread 대신 `workerIndex`, `worker`를 가진다.
  - `FContentRuntime::Start()`는 `workerThreadCount`만큼 worker를 만들고, 등록된 content instance를 정렬 후 `round-robin`으로 배치한다.
  - session route도 `contentInstanceId + worker`를 함께 가진다.

### 3-2. worker
- [FContentThread.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Threading\FContentThread.h)
- [FContentThread.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Threading\FContentThread.cpp)
  - 이름은 아직 `FContentThread`지만 의미는 `content worker thread`다.
  - worker 하나가 여러 content instance를 `RegisterContent()`로 등록받는다.
  - queue item에는 반드시 `contentInstanceId`가 같이 들어오고, worker는 자기 내부 registry에서 대상 content를 찾아 `Enter / Leave / Packet / Frame`를 처리한다.
  - `GetStatsSnapshot(contentInstanceId)`도 worker가 content instance별 상태를 찾아 반환한다.

### 3-3. envelope / lifecycle event
- [ContentRuntimeTypes.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Core\ContentRuntimeTypes.h)
  - `FOwnedPacketEnvelope`
  - `SContentLifecycleEvent`
  - 둘 다 `contentInstanceId`를 명시적으로 가진다.
- 이제 queue item만 보고도 worker가 어떤 content instance를 처리해야 하는지 알 수 있다.

## 4. 설정
- [EchoServer.schema.yaml](D:\Project\ServerPortfolio\RefactoringServer\ConfigSchema\Server\EchoServer.schema.yaml)
- [EchoServer.yaml](D:\Project\ServerPortfolio\RefactoringServer\Config\Server\EchoServer.yaml)
- [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)

새 설정:
- `EchoServer.ContentsWorkerThreadCount`
- CLI override: `--contents-worker-thread-count`

이 값은 `SContentRuntimeConfig.workerThreadCount`로 내려가고, content worker pool 크기를 결정한다.

## 5. EchoServer 기준 흐름
1. `Main.cpp`가 `ContentsWorkerThreadCount`를 읽어 `SContentRuntimeConfig.workerThreadCount`에 넣는다.
2. `FContentRuntime::RegisterContent()`는 content instance를 slot에 등록만 한다.
3. `FContentRuntime::Start()`가 worker pool을 만들고 instance를 `round-robin`으로 worker에 배치한다.
4. 세션 라우트는 `contentInstanceId`와 그 instance가 속한 worker를 가리킨다.
5. `EnqueuePacket`, `EnterSessionToInstance`, `MoveSessionToInstanceWithCompletion`, `LeaveSession`은 모두 해당 worker queue로 work item을 넣는다.
6. worker는 queue item의 `contentInstanceId`를 보고 정확한 `IContent` 인스턴스에 dispatch한다.

## 6. 검증
### 6-1. 빌드
- `Debug x64` 솔루션 빌드 성공

### 6-2. 스레드 수 확인
- 조건:
  - `room-count=80`
  - `worker-thread-count=2`
  - `contents-worker-thread-count=4`
- 서버 프로세스 thread count: `10`
- 로그:
  - [content_worker_pool_threadcount](D:\Project\ServerPortfolio\RefactoringServer\Out\content_worker_pool_threadcount)

즉 `Room 80개 = thread 80개`가 아니라, room instance는 그대로 유지하면서 thread 수는 worker 수 기준으로 제한된다.

### 6-3. 기능 스모크
- `20 sessions / 10s` 성공
  - [client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\content_worker_pool_smoke\client.log)

### 6-4. 짧은 회귀
- `100 sessions / 30s` 성공
  - [client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\content_worker_pool_regression_100x30s\client.log)
  - [server.log](D:\Project\ServerPortfolio\RefactoringServer\Out\content_worker_pool_regression_100x30s\server.log)

관찰된 내용:
- `ContentStats` 기준 `contents=82`를 유지한 채 동작했다.
- `roomFrameTPS`는 약 `2400` 수준으로 유지됐다.
- `roomQueue`는 `0`, `roomMaxQueue`는 `3` 수준이었다.

## 7. 현재 한계
- 배치 정책은 아직 `round-robin` 고정이다.
- worker 이름은 아직 `FContentThread`라서 의미가 조금 옛 구조를 끌고 간다.
- 동적 재배치, load-aware assignment, work stealing은 아직 없다.

## 8. 결론
- 1차 목표였던 `instance-thread 결합 해소`는 달성했다.
- 이제 `ContentsRuntime`는 레거시의 content worker pool 모델에 더 가까운 구조가 되었고, 다음 `NetworkLib` 성능 비교는 이전보다 훨씬 덜 왜곡된 기준선 위에서 볼 수 있다.
