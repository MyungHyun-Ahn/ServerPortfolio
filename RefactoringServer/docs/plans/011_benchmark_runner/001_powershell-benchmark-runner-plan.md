# PowerShell BenchmarkRunner Plan

## 1. 목적
- `RefactoringServer`의 벤치마크 실행을 수동 순서 실행에서 벗어나 `PowerShell` 스크립트 기반 자동 실행으로 정리한다.
- 실행 대상은 1차로 `ChattingServer + ChattingDummyClient`이지만, 구조는 `Echo`, 향후 다른 더미 클라이언트, 다른 서버 샘플에도 재사용 가능한 범용 러너로 설계한다.
- 실행할 실험 조합은 코드 수정이 아니라 `YAML manifest` 파일로 제어한다.

## 2. 배경
- 현재 레포에는 [Run-RioDispatchComparisonSequence.ps1](D:\Project\ServerPortfolio\RefactoringServer\scripts\Run-RioDispatchComparisonSequence.ps1), [Run-SndBufBackendComparisonSequence.ps1](D:\Project\ServerPortfolio\RefactoringServer\scripts\Run-SndBufBackendComparisonSequence.ps1)처럼 특정 실험을 위해 작성된 순차 실행 스크립트가 이미 존재한다.
- 이 스크립트들은 `config template 복사`, `YAML scalar patch`, `프로세스 실행`, `로그 수집`, `결과 폴더 생성` 같은 핵심 패턴을 이미 갖고 있다.
- 하지만 현재 구조는 실험 대상과 지표가 스크립트에 하드코딩되어 있어, `ChattingServer`처럼 새로운 시나리오를 추가할 때마다 별도 스크립트를 복제하는 방향으로 흐를 가능성이 높다.
- `ChattingDummyClient`가 준비된 시점부터는 동일한 실험을 `RIO`, `IOCP`, 큰 payload, room selection mode 조합별로 반복 실행할 수 있는 상위 실행기가 필요하다.

## 3. 목표
- `PowerShell` 기반 `BenchmarkRunner`를 추가한다.
- `YAML manifest` 1개로 서로 다른 `N개 run case`를 순차 실행할 수 있어야 한다.
- 공통 실행기와 시나리오별 어댑터를 분리한다.
- 1차 어댑터는 `ChattingScenario`로 구현한다.
- 결과는 `run directory` 단위로 아카이브되고, 최종 집계는 `csv/json`으로 남긴다.

## 4. 비목표
- 1차에 실시간 웹 대시보드까지 만들지 않는다.
- 1차에 다중 머신 분산 벤치마크를 지원하지 않는다.
- 1차에 모든 YAML 기능(anchor, merge key, multiline block scalar)을 지원하지 않는다.
- 1차에 벤치마크 러너 자체를 별도 C++ 바이너리로 만들지 않는다.

## 5. 핵심 결정
- 실행기는 `PowerShell`로 만든다.
- 실험 정의는 `YAML manifest`로 둔다.
- `BenchmarkRunner Core`는 실행 순서와 공통 수집만 담당한다.
- 서버/클라이언트별 config patch, readiness 판단, 결과 요약 파싱은 `Scenario Adapter`가 담당한다.
- 1차는 `Runs: []` 배열을 명시적으로 순차 실행한다.
- 조합 자동 생성용 `Matrix`는 후속 단계로 둔다.

## 6. 전체 구조
### 6.1 디렉터리
- `scripts/bench/Run-Benchmark.ps1`
- `scripts/bench/Benchmark.Common.ps1`
- `scripts/bench/scenarios/Invoke-ChattingScenario.ps1`
- `scripts/bench/manifests/*.yaml`
- `Out/bench/<timestamp>/<run-name>/...`

### 6.2 역할 분리
- `Run-Benchmark.ps1`
  - manifest 로드
  - 공통 defaults 병합
  - `Runs` 배열 순차 실행
  - 실패 정책 처리
  - 전체 집계 파일 기록
- `Benchmark.Common.ps1`
  - YAML patch helper
  - 임시 config 생성
  - 프로세스 시작/종료
  - timeout / orphan cleanup
  - stdout/stderr redirect
  - 결과 디렉터리 구성
- `Invoke-ChattingScenario.ps1`
  - `ChattingServer.yaml`, `ChattingDummy.yaml` template 기반 effective config 생성
  - 서버 readiness 대기
  - 더미 실행
  - `summary`, `RTT CSV`, `server log` 파싱

## 7. Manifest 모델
### 7.1 1차 구조
- `Scenario`
- `OutputRoot`
- `ContinueOnError`
- `Defaults`
  - 서버 공통 기본값
  - 클라이언트 공통 기본값
  - 타이밍 공통 기본값
- `Runs`
  - 이름
  - 태그
  - enabled 여부
  - 서버 override
  - 클라이언트 override
  - 반복 횟수

### 7.2 예시
```yaml
Scenario: Chatting
OutputRoot: Out/bench
ContinueOnError: true

Defaults:
  Timing:
    WarmupSeconds: 10
    MeasureSeconds: 60
    CooldownSeconds: 5
    StartupTimeoutSeconds: 10
    ShutdownTimeoutSeconds: 10
  Server:
    Executable: Out/ChattingServer.exe
    ConfigTemplate: Config/Server/ChattingServer.yaml
    Overrides:
      ChattingServer.Port: 19100
      Debug.Headless: true
  Client:
    Executable: Out/ChattingDummyClient.exe
    ConfigTemplate: Config/Client/ChattingDummy.yaml
    Overrides:
      ChattingDummy.ServerIp: 127.0.0.1
      ChattingDummy.Port: 19100
      ChattingDummy.PacketKey: 55
      ChattingDummy.RunSeconds: 60

Runs:
  - Name: iocp_8k_200_hotspot
    Tags: [iocp, large-packet, hotspot]
    RepeatCount: 3
    Server:
      Overrides:
        ChattingServer.Backend: Iocp
        ChattingServer.MaxChatPayloadBytes: 8000
    Client:
      Overrides:
        ChattingDummy.SessionCount: 200
        ChattingDummy.SendIntervalMs: 0
        ChattingDummy.PayloadSizeBytes: 8000
        ChattingDummy.RoomSelectionMode: Hotspot
        ChattingDummy.HotspotRoomIds: "77"
        ChattingDummy.HotspotBiasPercent: 80

  - Name: rio_direct_8k_200_hotspot
    Tags: [rio, large-packet, hotspot]
    RepeatCount: 3
    Server:
      Overrides:
        ChattingServer.Backend: Rio
        ChattingServer.RioSendDispatchMode: Direct
        ChattingServer.MaxChatPayloadBytes: 8000
    Client:
      Overrides:
        ChattingDummy.SessionCount: 200
        ChattingDummy.SendIntervalMs: 0
        ChattingDummy.PayloadSizeBytes: 8000
        ChattingDummy.RoomSelectionMode: Hotspot
        ChattingDummy.HotspotRoomIds: "77"
        ChattingDummy.HotspotBiasPercent: 80
```

## 8. 실행 수명주기
1. manifest를 읽고 `Defaults + Run Override`를 병합한다.
2. 전체 실행 루트 디렉터리를 생성한다.
3. 각 run마다 고유 결과 디렉터리를 만든다.
4. server/client effective config 파일을 결과 디렉터리에 생성한다.
5. 서버를 headless 모드로 실행한다.
6. readiness timeout 동안 서버가 정상 기동했는지 확인한다.
7. 더미 클라이언트를 실행한다.
8. run 종료 또는 timeout까지 대기한다.
9. stdout/stderr, effective config, RTT CSV, log file, summary file을 결과 디렉터리에 보관한다.
10. 서버와 클라이언트 프로세스를 정리한다.
11. 다음 run으로 이동한다.

## 9. 범용성 설계 원칙
### 9.1 Runner Core는 채팅을 모른다
- core는 `ChattingServer`, `EchoServer`, `WorldServer` 같은 이름을 하드코딩하지 않는다.
- core는 `ProcessSpec`, `ConfigTemplate`, `Overrides`, `Timing`, `ArtifactPolicy` 같은 공통 개념만 안다.

### 9.2 Scenario Adapter가 도메인을 안다
- `ChattingScenario`는 `ChattingServer` config key 이름을 안다.
- `EchoScenario`는 `EchoServer` config key 이름을 안다.
- 결과 파싱도 scenario별로 분리한다.

### 9.3 기존 스크립트 재사용
- 기존 `Run-RioDispatchComparisonSequence.ps1`, `Run-SndBufBackendComparisonSequence.ps1`의 `Set-YamlScalarValue`와 process orchestration 패턴을 공통 helper로 끌어올린다.
- 새 러너가 안정화되면 기존 단발성 비교 스크립트를 점진적으로 manifest 기반 호출로 이관한다.

## 10. Chatting 1차 적용 범위
### 10.1 서버 측 제어 항목
- `ChattingServer.Backend`
- `ChattingServer.RioSendDispatchMode`
- `ChattingServer.WorkerThreadCount`
- `ChattingServer.MaxSessionCount`
- `ChattingServer.ContentsWorkerThreadCount`
- `ChattingServer.RoomCount`
- `ChattingServer.RoomCapacity`
- `ChattingServer.MaxChatPayloadBytes`
- `ChattingServer.SocketSendBufferBytes`
- `Debug.Headless`
- `ChattingServer.LogOutputDirectory`

### 10.2 더미 측 제어 항목
- `ChattingDummy.SessionCount`
- `ChattingDummy.ConnectsPerSecond`
- `ChattingDummy.RunSeconds`
- `ChattingDummy.SendIntervalMs`
- `ChattingDummy.PayloadSizeBytes`
- `ChattingDummy.RoomSelectionMode`
- `ChattingDummy.HotspotRoomIds`
- `ChattingDummy.HotspotBiasPercent`
- `ChattingDummy.RoomChangeProbabilityPercent`
- `ChattingDummy.ReconnectProbabilityPercent`
- `ChattingDummy.ReconnectDelayMs`
- `ChattingDummy.ResponseTimeoutMs`
- `ChattingDummy.RttCsvPath`

### 10.3 1차 수집 지표
- `connectSuccess`
- `loginSuccess`
- `roomListResponses`
- `roomChangeSuccess`
- `roomChangeFailure`
- `chattingSend`
- `chattingSuccess`
- `chattingReject`
- `broadcastReceive`
- `reconnect`
- `unexpectedDisconnect`
- `timeout`
- `selfBroadcast`
- `invalidRoomBroadcast`
- `payloadValidationFailure`
- `permanentFailure`
- `RTT p50/p95/p99`
- server log 기반 `sendTPS`, `recvTPS`, `cpuPercent`, `workingSetMB`

## 11. 결과물 구조
### 11.1 run 디렉터리
- `manifest.snapshot.yaml`
- `effective.server.yaml`
- `effective.client.yaml`
- `server.stdout.log`
- `server.stderr.log`
- `client.stdout.log`
- `client.stderr.log`
- `rtt.csv`
- `run-summary.json`

### 11.2 sequence 루트
- `sequence-summary.csv`
- `sequence-summary.json`
- `failed-runs.txt`

## 12. 실패 정책
- `ContinueOnError`
  - true면 한 run 실패 후 다음 run 계속 진행
  - false면 즉시 중단
- `StartupTimeoutSeconds`
  - 서버 기동 실패 판단 시간
- `ShutdownTimeoutSeconds`
  - graceful 종료 대기 시간
- `RunTimeoutSeconds`
  - client hang 또는 server stall 대비 hard timeout
- orphan process가 남으면 강제 종료 후 실패 기록

## 13. Manifest / YAML 전략
- 1차 manifest는 사람이 직접 읽고 편집하기 쉬운 `YAML` 형식을 유지한다.
- 1차 지원 범위는 scalar, flat list, 2단계 map 중심의 제한형 구조로 둔다.
- 복잡한 YAML 기능은 지원 범위에서 제외한다.
- 러너 본체는 `manifest object` 기반으로 동작하게 만들고, manifest loader 구현은 교체 가능하게 유지한다.
- 즉 `YAML loader` 구현 방식이 바뀌더라도 `Run-Benchmark.ps1`의 실행 코어는 유지되도록 설계한다.

## 14. 구현 순서
1. `scripts/bench` 디렉터리와 공통 helper 뼈대를 추가한다.
2. 기존 비교 스크립트의 YAML patch / process control helper를 공통 모듈로 추출한다.
3. manifest loader를 붙인다.
4. `Runs[]` 순차 실행 루프를 구현한다.
5. `ChattingScenario` adapter를 추가한다.
6. `run-summary.json`, `sequence-summary.csv`를 기록한다.
7. `ChattingServer / ChattingDummyClient`용 대표 manifest를 추가한다.
8. 기존 `Echo` 비교 스크립트를 새 runner 기반으로 점진 이관한다.

## 15. 기대 효과
- `RIO / IOCP` 비교를 사람 손으로 반복 실행하지 않아도 된다.
- 실험 케이스 추가가 스크립트 복제가 아니라 manifest 추가로 바뀐다.
- `Chatting`뿐 아니라 다른 성능 실험에도 같은 실행기를 재사용할 수 있다.
- 벤치마크 재현성과 실행 기록 보존 수준이 올라간다.
