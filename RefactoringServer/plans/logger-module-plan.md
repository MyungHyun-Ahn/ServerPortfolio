# Logger Module Plan

## 1. 목적
- `RefactoringServer`에서 사용할 새 로거 모듈을 별도로 설계한다.
- 기존 `MHLib::utils::CLogger`의 경험은 참고하되, 현재 아키텍처와 코딩 규칙에 맞는 형태로 다시 만든다.
- 초기 목표는 `NetworkLib`, `EchoServer`, 테스트 도구들이 공통으로 사용할 수 있는 예측 가능한 로깅 기반을 확보하는 것이다.

## 2. 새로 만드는 이유
- 기존 [`CLogger.h`](D:\Project\ServerPortfolio\includes\MHLib\utils\CLogger.h)는 다음 이유로 직접 이식하기 어렵다.
  - `Singleton` 기반 전역 접근을 전제로 한다.
  - `g_Logger` 전역 포인터를 사용한다.
  - 파일 경로, 디렉터리 구성, UTF-16LE 파일 출력 방식이 기존 프로젝트 관례에 강하게 결합되어 있다.
  - `RefactoringServer`의 새 계층 경계와 테스트 구조에서는 의존 주입 또는 명시적 초기화 방식이 더 적합하다.

## 3. 목표 범위
### 3-1. 1차 목표
- 콘솔 출력
- 파일 출력
- 로그 레벨 분리
- 모듈 이름 또는 카테고리 구분
- 멀티스레드 안전 출력
- `NetworkLib`와 샘플 서버에서 공통 사용 가능

### 3-2. 아직 제외할 것
- 원격 로그 수집
- JSON 로그 포맷
- 고급 log rotation
- 실시간 모니터링 시스템 연동
- 외부 로그 라이브러리 연동

## 4. 설계 원칙
- 전역 singleton을 기본값으로 두지 않는다.
- 로거 인스턴스의 생성과 수명주기는 상위 애플리케이션이 명시적으로 결정한다.
- 코어 경로에서 로깅이 병목이 되지 않도록, 동기 파일 쓰기와 비동기 로그 큐 중 어떤 모델을 쓸지 초기 단계에서 구분한다.
- 로그 API는 테스트 코드에서도 쉽게 대체 가능해야 한다.
- 설명 주석과 문서는 한국어 기준으로 유지한다.

## 5. 제안 구조
### 5-1. 인터페이스
- `ILogger`
  - `Log(ELogLevel level, std::wstring_view category, std::wstring_view message)`
  - 또는 포맷팅 유틸리티를 감싼 helper 제공

### 5-2. 구현 후보
- `FConsoleLogger`
  - 콘솔 출력 전용
- `FFileLogger`
  - 파일 출력 전용
- `FCompositeLogger`
  - 여러 sink에 동시에 기록

### 5-3. 데이터 정의
- `ELogLevel`
  - `Debug`, `Info`, `Warn`, `Error`
- `SLogConfig`
  - 최소 로그 레벨
  - 출력 디렉터리
  - 파일명 규칙
  - 콘솔 출력 여부

## 6. 1차 구현 방향
### 6-1. 간단한 시작점
- 첫 버전은 `FFileLogger + FConsoleLogger + FCompositeLogger` 조합이 가장 무난하다.
- 파일 출력은 UTF-8 텍스트를 기본으로 한다.
- 파일명 규칙은 `logs/<module>/<yyyyMMdd>_<category>.log` 정도의 단순한 구조로 시작한다.

### 6-2. 동기화 방향
- 초기 버전은 `SRWLOCK` 또는 `std::mutex` 기반의 단순한 출력 직렬화를 허용한다.
- 이유
  - 로거는 lock-free 코어 자체가 아니라 운영 보조 모듈이다.
  - 여기서 억지로 lock-free를 강제하면 복잡도만 올라가고, 실제 이득 근거가 부족하다.
- 다만 향후 hot path 로그가 많아지면 `lock-free queue + logging thread` 모델을 2차 확장으로 검토한다.

### 6-3. 포맷 기준
- 한 줄 형식 예시
  - `[NetworkLib] [2026-04-01 22:15:10] [Info] message`
- 필수 필드
  - category
  - timestamp
  - level
  - message
- 선택 필드
  - thread id
  - session id

## 7. 적용 순서
### 7-1. 1단계
- `ILogger`, `ELogLevel`, `SLogConfig` 정의
- 콘솔/파일 출력 구현
- `EchoServer`에서 시작/종료 로그 적용

### 7-2. 2단계
- `FIocpServer` 주요 경계 로그 연결
  - 서버 시작
  - listen 실패
  - accept/recv/send 실패
  - 세션 종료

### 7-3. 3단계
- 테스트 도구와 soak 테스트에도 공통 로그 경로 규칙 적용
- 필요하면 비동기 sink 설계 검토

## 8. 검증 계획
- 빌드 검증
- `EchoServer` 실행 시 콘솔/파일 로그 생성 확인
- 다중 로그 호출 시 줄 단위 깨짐 여부 확인
- 잘못된 경로 또는 파일 오픈 실패 시 graceful fallback 확인

## 9. 확정된 사실
- `RefactoringServer`에는 공통 로거 모듈이 하나 필요하다.
- 기존 `CLogger`는 직접 이식보다 재설계가 더 안전하다.
- 첫 버전은 운영 보조 모듈로 보고, 단순하고 읽기 쉬운 구조를 우선하는 편이 맞다.

## 10. 다음 확인 항목
- 로거를 `NetworkLib` 내부 유틸리티로 둘지, 상위 `RefactoringServer` 공용 모듈로 뺄지 결정
- 문자열 타입을 `std::wstring` 중심으로 갈지, UTF-8 `std::string` 중심으로 갈지 결정
- 초기 버전에서 로그 회전까지 넣을지 여부 결정
