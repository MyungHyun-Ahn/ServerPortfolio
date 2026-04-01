# Logger Module Plan

## 1. 목적
- `RefactoringServer/Foundation/Logging`에서 사용할 공용 로거 모듈을 설계한다.
- 기존 `MHLib::utils::CLogger`의 경험을 참고하되, 현재 구조와 코딩 규칙에 맞는 형태로 다시 만든다.
- `NetworkLib`, `EchoServer`, `Diagnostics`, 테스트 도구가 함께 쓸 수 있는 공용 로깅 기반을 확보한다.

## 2. 새로 만드는 이유
- 기존 [`CLogger.h`](D:\Project\ServerPortfolio\includes\MHLib\utils\CLogger.h)는 다음 이유로 직접 이식하기 어렵다.
  - `Singleton` 기반 전역 접근을 전제로 한다.
  - `g_Logger` 전역 포인터에 의존한다.
  - 기존 프로젝트 경로, 파일 배치, UTF-16 출력 방식과 강하게 결합되어 있다.
  - 현재 `RefactoringServer`의 명시적 초기화와 의존성 주입 방향과 맞지 않는다.

## 3. 배치 기준
- 로거는 `NetworkLib` 내부 모듈이 아니라 공용 기반 모듈로 둔다.
- 최종 디렉터리 기준은 `RefactoringServer/Foundation/Logging`이다.
- 이유:
  - `Diagnostics`와 여러 서버 프로젝트가 함께 사용할 가능성이 높다.
  - 공용 운영 모듈이 네트워크 코어에 역의존하지 않도록 하기 위함이다.

## 4. 목표 범위

### 4-1. 1차 목표
- 콘솔 출력
- 파일 출력
- 로그 레벨 분리
- 카테고리 구분
- 스레드 ID 포함 여부 제어
- 여러 sink 동시 출력

### 4-2. 1차 제외 범위
- 원격 로그 수집
- JSON 로그 포맷
- 고급 rotation
- 비동기 전용 logging thread
- 외부 로깅 라이브러리 연동

## 5. 제안 구조

### 5-1. 인터페이스
- `ILogger`
  - `Log(ELogLevel level, std::string_view category, std::string_view message)`

### 5-2. 구현 후보
- `FConsoleLogger`
- `FFileLogger`
- `FCompositeLogger`

### 5-3. 공통 타입
- `ELogLevel`
  - `Debug`, `Info`, `Warn`, `Error`
- `SLogConfig`
  - 최소 로그 레벨
  - 출력 디렉터리
  - 콘솔/파일 출력 여부
  - 스레드 ID 포함 여부

## 6. 1차 구현 방향
- 첫 버전은 `FConsoleLogger + FFileLogger + FCompositeLogger` 조합으로 시작한다.
- 파일 출력은 UTF-8 텍스트 기반으로 단순하게 간다.
- 로그 경로는 `logs/<category>/<yyyyMMdd>_<category>.log` 정도의 단순 규칙으로 시작한다.
- 동기식 출력과 간단한 mutex 직렬화를 기본으로 한다.
  - 로거는 lock-free 코어 자체가 아니라 운영 보조 모듈이다.
  - 초기 단계에서는 성능보다 예측 가능성과 디버깅 용이성이 더 중요하다.

## 7. 적용 순서
1. `ILogger`, `ELogLevel`, `SLogConfig` 정의
2. 콘솔/파일 sink 구현
3. `EchoServer`에 적용
4. `NetworkLib` 주요 경계 로그 연결
5. 현재 `NetworkLib/Logging` 구현을 `Foundation/Logging`으로 이동

## 8. 검증 계획
- 빌드 검증
- `EchoServer` 실행 시 콘솔/파일 로그 생성 확인
- 다중 로그 호출 시 형식 깨짐 여부 확인
- 잘못된 파일 경로 또는 파일 열기 실패 시 graceful fallback 확인

## 9. 현재 결론
- 공용 로거 모듈은 필요하다.
- 기존 `CLogger`를 직접 이식하기보다 재설계가 맞다.
- 로거는 `NetworkLib` 내부보다 `Foundation/Logging`에 두는 편이 구조상 자연스럽다.
