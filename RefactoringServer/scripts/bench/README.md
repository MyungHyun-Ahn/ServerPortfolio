# Benchmark Scripts

## 1. 목적
- `scripts/bench`는 `RefactoringServer`의 벤치마크를 `PowerShell`로 반복 실행하기 위한 스크립트 디렉터리다.
- 현재 1차 지원 시나리오는 `ChattingServer + ChattingDummyClient`이다.
- 실행 대상, 기본 설정, run case 조합은 `YAML manifest`로 제어한다.

## 2. 현재 구성
- [Run-Benchmark.ps1](D:\Project\ServerPortfolio\RefactoringServer\scripts\bench\Run-Benchmark.ps1)
  - 메인 실행기
- [Run-Benchmark.cmd](D:\Project\ServerPortfolio\RefactoringServer\scripts\bench\Run-Benchmark.cmd)
  - `PowerShell` 래퍼
- [Benchmark.Common.ps1](D:\Project\ServerPortfolio\RefactoringServer\scripts\bench\Benchmark.Common.ps1)
  - 공통 helper
  - 제한형 YAML 파서
  - config patch
  - process 실행/정리
- [Invoke-ChattingScenario.ps1](D:\Project\ServerPortfolio\RefactoringServer\scripts\bench\scenarios\Invoke-ChattingScenario.ps1)
  - `Chatting` 시나리오 어댑터
- [chatting-smoke.yaml](D:\Project\ServerPortfolio\RefactoringServer\scripts\bench\manifests\chatting-smoke.yaml)
  - 최소 smoke 예시 manifest

## 3. 빠른 시작
### PowerShell
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\bench\Run-Benchmark.ps1 `
  -Manifest scripts\bench\manifests\chatting-smoke.yaml `
  -OutputLabel smoke
```

### CMD
```bat
scripts\bench\Run-Benchmark.cmd -Manifest scripts\bench\manifests\chatting-smoke.yaml -OutputLabel smoke
```

권장 실행 위치:
- `RefactoringServer` 루트에서 실행

필수 선행 조건:
- [ChattingServer.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\ChattingServer.exe)가 빌드되어 있어야 한다.
- [ChattingDummyClient.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\ChattingDummyClient.exe)가 빌드되어 있어야 한다.
- 기본 template config가 존재해야 한다.
  - [ChattingServer.yaml](D:\Project\ServerPortfolio\RefactoringServer\Config\Server\ChattingServer.yaml)
  - [ChattingDummy.yaml](D:\Project\ServerPortfolio\RefactoringServer\Config\Client\ChattingDummy.yaml)

## 4. 실행 방식
1. manifest를 읽는다.
2. `Defaults`와 각 `Runs[]` 항목을 병합한다.
3. run마다 임시 `effective.server.yaml`, `effective.client.yaml`을 만든다.
4. 서버를 headless 모드로 실행한다.
5. 더미 클라이언트를 실행한다.
6. 종료 후 stdout/stderr, 요약 JSON, sequence CSV를 저장한다.

## 5. Manifest 구조
### 기본 예시
```yaml
Scenario: Chatting
OutputRoot: Out/bench
ContinueOnError: false

Defaults:
  Timing:
    WarmupSeconds: 0
    MeasureSeconds: 5
    CooldownSeconds: 0
    StartupTimeoutSeconds: 10
    RunTimeoutSeconds: 20
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

Runs:
  - Name: iocp_smoke
    Server:
      Overrides:
        ChattingServer.Backend: Iocp
    Client:
      Overrides:
        ChattingDummy.SessionCount: 8
        ChattingDummy.PayloadSizeBytes: 256
```

### 주요 필드
- `Scenario`
  - 현재는 `Chatting`만 지원
- `OutputRoot`
  - 결과 디렉터리 루트
- `ContinueOnError`
  - `true`면 한 run 실패 후 다음 run 계속 진행
- `Defaults`
  - 모든 run에 공통 적용될 기본값
- `Runs`
  - 실제 순차 실행할 run 목록
- `RepeatCount`
  - 동일 run을 여러 번 반복하고 싶을 때 사용
- `Enabled`
  - 임시 비활성화할 run에 사용

## 6. Override 규칙
- override key는 `<Section>.<Key>` 형식만 지원한다.
- 예:
  - `ChattingServer.Backend`
  - `ChattingServer.MaxChatPayloadBytes`
  - `ChattingDummy.SessionCount`
  - `ChattingDummy.RoomSelectionMode`

현재는 template YAML 안에 이미 존재하는 key만 덮어쓸 수 있다.
- 없는 section/key를 새로 추가하는 기능은 아직 없다.

## 7. 결과물 위치
예시:
- `Out/bench/20260407_164920_smoke_test3/`

sequence 루트에는 아래 파일이 생긴다.
- `manifest.snapshot.yaml`
- `sequence-summary.csv`
- `sequence-summary.json`
- `failed-runs.txt`

각 run 디렉터리에는 아래 파일이 생긴다.
- `effective.server.yaml`
- `effective.client.yaml`
- `server.stdout.log`
- `server.stderr.log`
- `client.stdout.log`
- `client.stderr.log`
- `rtt.csv`
- `run-summary.json`

## 8. 현재 지원 범위
- `Scenario: Chatting`
- `Runs[]` 기반 순차 실행
- `RepeatCount`
- `ContinueOnError`
- `ChattingDummyClient` 최종 summary 파싱
- `ChattingServer`의 `ChattingStats`, `ContentStats` 마지막 줄 파싱

## 9. 현재 제한 사항
- YAML은 “제한형 파서”다.
- 현재 잘 되는 범위:
  - scalar
  - 중첩 map
  - flat list
  - `Runs:` 아래 `- Name: ...` 구조
- 아직 미지원 또는 비권장:
  - anchor / alias
  - merge key
  - 복잡한 multiline block scalar
  - template에 없는 key 동적 추가
- 현재 지원 시나리오는 `Chatting` 하나뿐이다.

## 10. 자주 쓰는 실행 예
### Smoke
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\bench\Run-Benchmark.ps1 `
  -Manifest scripts\bench\manifests\chatting-smoke.yaml `
  -OutputLabel smoke
```

### 다른 결과 라벨로 저장
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\bench\Run-Benchmark.ps1 `
  -Manifest scripts\bench\manifests\chatting-smoke.yaml `
  -OutputLabel iocp_baseline
```

## 11. 다음 확장 포인트
- `Chatting` 실제 비교 manifest 추가
  - `Iocp`
  - `Rio Direct`
  - `Rio OwnerThread`
- `RepeatCount=3` 이상 실험 preset 추가
- `Echo` 시나리오 어댑터 추가
- manifest `Matrix` 자동 확장 지원
