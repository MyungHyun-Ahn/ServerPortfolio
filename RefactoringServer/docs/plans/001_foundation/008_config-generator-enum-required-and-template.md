# ConfigGenerator Enum, Required, Template Plan

## 1. 목적
- `ConfigGenerator`가 문자열 기반 설정만 생성하는 상태에서 한 단계 더 나아가 `enum`, `required`, 실행용 sample YAML 자동 생성을 지원하도록 확장한다.
- `EchoServer`, `EchoClient`가 더 이상 수동 문자열 파싱과 수동 YAML 유지보수에 의존하지 않도록 한다.

## 2. 목표
- schema에서 `enum`을 선언하면 generated C++ enum과 enum reader 코드를 함께 생성한다.
- schema에서 `required: true`를 선언하면 generated loader가 `ReadRequired*` 경로를 사용한다.
- schema를 기준으로 실행용 sample YAML도 자동 생성한다.
- 기본 sample YAML을 그대로 실행해도 기존 `--config` 없는 기본 실행 흐름이 유지되어야 한다.

## 3. 범위
- `Tools/ConfigGenerator`
- `Foundation/Config`
- `Generated/Config/**`
- `Config/**`
- `EchoServer`의 수동 문자열 파싱 제거 범위

## 4. 스키마 확장 규칙
```yaml
EchoServer:
  Backend:
    type: enum
    default: Iocp
    values: [Iocp, Rio, BoostAsio]

  Port:
    type: uint16
    default: 19000

Debug:
  Headless:
    type: bool
    default: false
```

### 규칙
- `enum` 필드는 `type: enum`과 `values`를 함께 가진다.
- `default`는 `values` 안의 값과 일치해야 한다.
- `required: true`는 generated loader에서 필수 키로 처리한다.
- `required + default` 조합은 기본 sample YAML 실행 흐름을 유지하면서도 “키가 반드시 존재해야 한다”는 정책을 함께 표현한다.
- `required + default 없음`은 사람이 값을 채워야 하는 배포/운영 전용 항목에 사용한다.

## 5. 생성 규칙
- 파일명에서 target을 추론한다.
  - `EchoServer.schema.yaml` -> `EchoServer`
- generated header는 다음을 포함한다.
  - section struct
  - root document
  - enum field에 대응되는 generated enum
- generated cpp는 다음을 포함한다.
  - enum string-to-value 테이블
  - `ReadOptional*`, `ReadRequired*` 호출 코드
- 실행용 sample YAML은 schema 기본값을 그대로 반영한다.

## 6. 런타임 규칙
- `Foundation/Config::FConfigValueReader`는 enum 읽기 API를 제공한다.
- generated loader는 schema 메타데이터에 따라 자동으로 적절한 reader를 호출한다.
- 상위 프로젝트는 generated enum을 런타임 enum으로 변환만 담당한다.

## 7. 적용 방향
- `EchoServer`
  - `Backend`, `LogMinimumLevel`, race injection mode를 schema enum으로 승격한다.
  - `Main.cpp`의 수동 string parser를 generated enum 변환으로 대체한다.
  - `BindIp`, `Port`는 `required + default`로 관리한다.
- `EchoClient`
  - 현재 enum 후보는 적지만 sample YAML 자동 생성과 required 지원 체계는 동일하게 적용한다.
  - `ServerIp`, `Port`는 `required + default`로 관리한다.

## 8. 검증 기준
- `Generate-Configs.ps1`가 enum/required/sample YAML을 모두 정상 생성해야 한다.
- generated config 코드가 빌드돼야 한다.
- `EchoServer`, `EchoClient` 기본 YAML 자동 로딩 스모크가 성공해야 한다.
- schema의 enum default가 잘못되면 generator가 명확히 실패해야 한다.
- required 필드가 빠진 YAML은 generated loader에서 명확히 실패해야 한다.

## 9. 현재 결론
- `enum`, `required`, sample YAML 자동 생성은 Config 체계를 “실행 가능한 설정 시스템”으로 올리는 데 필요한 후속 작업이다.
- 이 범위가 끝나면 config 스키마는 구조 정의와 실행 템플릿의 단일 기준점이 된다.
- 현재 기본 endpoint 값은 `required + default`로 채택되어 더블클릭 실행과 필수 키 정책을 동시에 만족한다.
