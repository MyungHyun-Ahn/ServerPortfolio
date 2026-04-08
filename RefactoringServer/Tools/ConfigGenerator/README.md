# ConfigGenerator

간단한 YAML config schema를 읽어서 C++ 설정 문서/로더 코드를 생성하는 오프라인 도구다.

## 역할
- `RefactoringServer/ConfigSchema/**/*.schema.yaml` 스키마를 읽는다.
- `RefactoringServer/Generated/Config/**` 아래 generated C++ 코드를 만든다.
- 런타임 YAML과 대응되는 설정 문서 구조를 수동 struct 작성 없이 유지한다.

## 기본 실행
프로젝트 루트에서:

```powershell
RefactoringServer\scripts\generate\Generate-Configs.cmd
```

직접 실행:

```powershell
dotnet run --project RefactoringServer\Tools\ConfigGenerator\ConfigGenerator.csproj
```

## 인자
- `--schema-root <path>`
  - 기본값: `RefactoringServer/ConfigSchema`
- `--output-root <path>`
  - 기본값: `RefactoringServer/Generated/Config`
- `--config-root <path>`
  - 기본값: `RefactoringServer/Config`

예:

```powershell
dotnet run --project RefactoringServer\Tools\ConfigGenerator\ConfigGenerator.csproj -- --schema-root D:\Project\ServerPortfolio\RefactoringServer\ConfigSchema --output-root D:\Project\ServerPortfolio\RefactoringServer\Generated\Config --config-root D:\Project\ServerPortfolio\RefactoringServer\Config
```

## 스키마 형식
파일명에서 target을 추론한다.

- `ConfigSchema/Server/EchoServer.schema.yaml` -> `EchoServer`
- `ConfigSchema/Client/EchoClient.schema.yaml` -> `EchoClient`

스키마는 최상단 섹션 맵 형식으로 쓴다.

```yaml
EchoServer:
  Backend:
    type: enum
    default: Iocp
    values: [Iocp, Rio, BoostAsio]
  Port: { type: uint16, default: 19000 }

Debug:
  Headless: { type: bool, default: false }
  BootstrapTrace: { type: bool, default: false }
```

## 지원 필드 속성
- `type`
- `default`
- `required`
- `description`
- `values`

### `required` 사용 규칙
- `required: true`면 generated loader가 `ReadRequired*` 경로를 사용한다.
- `required + default`는 sample YAML에 기본값이 채워지므로 기본 실행을 유지하면서 필수 키 정책을 표현할 때 쓴다.
- `required + default 없음`은 사람이 직접 값을 채워야 하는 운영 전용 항목에 쓴다.

## 지원 타입
- `bool`
- `int32`, `uint16`, `uint32`, `int64`, `uint64`
- `float`, `double`
- `string`
- `enum`

## 생성 결과
- `Generated/Config/<Target>/<Target>Config.h`
- `Generated/Config/<Target>/<Target>Config.cpp`
- `Config/<relative-directory>/<Target>.yaml`

예:
- `Generated/Config/EchoServer/EchoServerConfig.h`
- `Generated/Config/EchoClient/EchoClientConfig.h`

## 생성 규칙
- 루트 클래스 이름은 자동 생성한다.
  - `EchoServer` -> `FEchoServerConfigDocument`
- 섹션 클래스 이름도 자동 생성한다.
  - `EchoServer` -> `SEchoServerConfig`
  - `Debug` -> `SEchoServerDebugConfig`
- enum field는 generated enum으로 승격된다.
- sample YAML은 schema 기본값과 enum 허용값 주석을 함께 생성한다.
- `required: true`인 field는 generated loader에서 `ReadRequired*` 경로를 사용한다.

예:

```yaml
EchoServer:
  BindIp: { type: string, default: 127.0.0.1, required: true }
  Port: { type: uint16, default: 19000, required: true }
```

## 운영 규칙
1. 스키마를 수정하면 `Generate-Configs.cmd`를 다시 실행한다.
2. generated C++ 코드는 커밋 대상이다.
3. `Tools/ConfigGenerator/bin`, `Tools/ConfigGenerator/obj`는 커밋 대상이 아니다.
4. 일반 C++ 프로젝트 빌드는 `ConfigGenerator`를 자동 실행하지 않는다.
5. 필요하면 [Generate-Codegen.cmd](D:\Project\ServerPortfolio\RefactoringServer\scripts\generate\Generate-Codegen.cmd)로 packet/config 생성기를 한 번에 실행한다.
