# YAML Config And ConfigGenerator Plan

## 1. 목적
- `EchoServer`, `EchoClient` 실행 옵션을 CLI 중심 구조에서 YAML 기반 구조로 전환한다.
- 실행 파일은 기본 설정 파일을 자동 탐색해 더블클릭만으로 실행 가능하게 만든다.
- 설정 스키마와 대응되는 C++ 설정 클래스를 수동으로 맞추지 않도록 `C# ConfigGenerator`로 생성한다.

## 2. 목표
- `Foundation/Config`에 공용 설정 로더를 둔다.
- 서버와 클라이언트는 서로 다른 YAML 파일을 사용한다.
- 스키마는 사람이 읽기 쉬운 최상단 섹션 맵 형식으로 관리한다.
- 스키마를 기준으로 generated C++ config 클래스를 만든다.
- 기본 실행은 YAML 자동 로드, 필요할 때만 `--config <path>`로 override 한다.

## 3. 디렉터리 구조
- 실행용 YAML
  - `RefactoringServer/Config/Server/EchoServer.yaml`
  - `RefactoringServer/Config/Client/EchoClient.yaml`
- 스키마
  - `RefactoringServer/ConfigSchema/Server/EchoServer.schema.yaml`
  - `RefactoringServer/ConfigSchema/Client/EchoClient.schema.yaml`
- 생성 코드
  - `RefactoringServer/Generated/Config/EchoServer/EchoServerConfig.h`
  - `RefactoringServer/Generated/Config/EchoServer/EchoServerConfig.cpp`
  - `RefactoringServer/Generated/Config/EchoClient/EchoClientConfig.h`
  - `RefactoringServer/Generated/Config/EchoClient/EchoClientConfig.cpp`
- 생성기
  - `RefactoringServer/Tools/ConfigGenerator`

## 4. 실행 정책
- `EchoServer.exe`, `EchoClient.exe`는 실행 파일 기준 상대 경로의 YAML을 자동 탐색한다.
- 기본 경로 탐색이 실패하면 명확한 에러 메시지를 출력하고 종료한다.
- 테스트나 임시 실행을 위해 `--config <path>`는 유지한다.

## 5. 실행용 YAML 형식
```yaml
EchoServer:
  Backend: Iocp
  BindIp: 127.0.0.1
  Port: 19000
  WorkerThreadCount: 2
  RoomCount: 50
  RoomCapacity: 4

Debug:
  Headless: false
  BootstrapTrace: false
  ContentsFailFast: false
```

```yaml
EchoClient:
  ServerIp: 127.0.0.1
  Port: 19000
  SessionCount: 1
  RequestCount: 1
  HoldSeconds: 0
  IntervalMs: 1000

Debug:
  Quiet: false
  RecvTimeoutMs: 0
  RoomListRecvTimeoutMs: -1
  EchoRecvTimeoutMs: -1
```

## 6. 스키마 형식
- `target` 제거
- `root-class` 제거
- `sections` 제거
- 파일명에서 target을 추론한다.
  - `EchoServer.schema.yaml` -> `EchoServer`
  - `EchoClient.schema.yaml` -> `EchoClient`

```yaml
EchoServer:
  Backend: { type: string, default: Iocp }
  Port: { type: uint16, default: 19000 }

Debug:
  Headless: { type: bool, default: false }
  BootstrapTrace: { type: bool, default: false }
```

### 형식 규칙
- 최상단 key는 섹션 이름이다.
- 섹션 내부 key는 필드 이름이다.
- 필드 값은 최소한 `type`을 가진다.
- 선택적으로 `default`, `required`, `description`을 가진다.

## 7. 생성 규칙
- 루트 설정 클래스 이름은 자동 생성한다.
  - `EchoServer.schema.yaml` -> `FEchoServerConfigDocument`
- 섹션 클래스 이름도 자동 생성한다.
  - `EchoServer` -> `SEchoServerConfig`
  - `Debug` -> `SEchoServerDebugConfig`
- 필드 이름은 스키마 key를 그대로 사용한다.

## 8. Foundation/Config 역할
- YAML 파일 로드
- 섹션/키 존재 여부 확인
- 타입 변환
- 알 수 없는 섹션/키 검증
- 에러 메시지 정리

도메인별 의미 해석은 상위 프로젝트가 담당한다.
- 예: `Backend: Iocp`를 enum으로 변환
- 예: `Headless`, `BootstrapTrace`를 런타임 옵션에 연결

## 9. ConfigGenerator 역할
- `*.schema.yaml` 탐색
- 파일명에서 target 추론
- 스키마 검증
- generated C++ header/cpp 생성
- 스키마 형식과 generated 코드의 일관성 유지

실행 예:
```powershell
dotnet run --project RefactoringServer\Tools\ConfigGenerator\ConfigGenerator.csproj
```

## 10. 1차 지원 타입
- `bool`
- `int32`, `uint16`, `uint32`, `int64`, `uint64`
- `float`, `double`
- `string`

## 11. 검증 기준
- `ConfigGenerator`가 최상단 섹션 맵 스키마를 정상 생성해야 한다.
- generated 코드가 빌드돼야 한다.
- `EchoServer`, `EchoClient`가 `--config` 없이 기본 YAML을 자동 로드해야 한다.
- `--config` override도 정상 동작해야 한다.
- 잘못된 키나 타입은 명확한 에러로 실패해야 한다.

## 12. 현재 결론
- 스키마는 최상단 섹션 맵 형식이 가장 읽기 쉽다.
- `target`, `root-class`, `sections`는 파일명과 생성 규칙으로 대체 가능하다.
- 현재 ConfigGenerator와 runtime은 이 형식에 맞춰 정리한다.
