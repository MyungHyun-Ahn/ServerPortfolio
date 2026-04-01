# Logger Module Review

## 1. 문서 목적
- `RefactoringServer/NetworkLib/Logging` 아래 1차 로거 모듈의 현재 구조와 판단 근거를 정리한다.
- 왜 이 모듈을 `util`이 아니라 `logging` 카테고리로 분리하는지 기록한다.
- 이후 `async logger`, `crash dump 연계`, `백엔드 공통 진단 로그`로 확장할 때 기준 문서로 사용한다.

## 2. 디렉터리 분류 판단
- 로거는 범용 도우미 함수 묶음보다, 출력 정책과 운영 관측을 담당하는 독립 모듈에 가깝다.
- 현재 코드도 `ILogger`, `FConsoleLogger`, `FFileLogger`, `FCompositeLogger`, `LogFormatting`처럼 역할이 명확히 묶여 있다.
- 따라서 `docs/reviews/util`보다는 `docs/reviews/logging`이 더 적합하다.
- 나중에 `metrics`, `crash dump`, `trace`가 생겨도 `logging` 카테고리에서 함께 찾는 편이 자연스럽다.

## 3. 현재 구현 범위
- 인터페이스:
  - [ILogger.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Logging\ILogger.h)
- 싱크 구현:
  - [FConsoleLogger.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Logging\FConsoleLogger.h)
  - [FFileLogger.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Logging\FFileLogger.h)
  - [FCompositeLogger.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Logging\FCompositeLogger.h)
- 공통 포맷:
  - [LogFormatting.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Logging\LogFormatting.h)
  - [LogFormatting.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Logging\LogFormatting.cpp)
- 서버 연결 지점:
  - [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FIocpServer.cpp)
  - [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)

## 4. 구조 판단 근거

### 4-1. 전역 singleton 대신 명시적 주입
- `SServerConfig`가 `SLogConfig`와 `std::shared_ptr<ILogger>`를 가진다.
- 서버가 전역 로거를 직접 찾지 않고 설정으로 주입받기 때문에 테스트와 교체가 쉽다.
- 기존 레거시 로거처럼 전역 singleton에 의존하지 않는다는 점이 현재 구조의 핵심 장점이다.

### 4-2. 출력 채널을 인터페이스로 분리
- `ILogger`는 `Log(ELogLevel, category, message)` 하나만 노출한다.
- 콘솔, 파일, 복합 로거가 같은 인터페이스를 구현하므로 호출측은 출력 채널을 몰라도 된다.
- 이 구조 덕분에 `EchoServer`는 복합 로거를 쓰고, 이후 테스트 환경에서는 null logger나 메모리 logger로 바꾸기 쉽다.

### 4-3. 포맷과 출력 책임 분리
- `FConsoleLogger`, `FFileLogger`는 직접 문자열 포맷을 만들지 않고 `Logging::BuildLine()`을 사용한다.
- 포맷 정책이 [LogFormatting.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Logging\LogFormatting.cpp)에 모여 있어, 로그 레이아웃 변경 시 수정 지점이 한 곳으로 줄어든다.
- 날짜 문자열과 스레드 ID 포함 여부도 같은 경로에서 결정된다.

### 4-4. 초기 단계는 동기식이 합리적
- 현재 `FConsoleLogger`, `FFileLogger`는 각각 `std::mutex` 기반 직렬화 출력이다.
- lock-free 코어 방향과 별개로, 로거는 운영 보조 모듈이므로 1차 구현에서 단순성과 디버깅 용이성을 우선한 판단은 타당하다.
- 특히 지금은 고성능 async logger보다 "실패 시 로그가 즉시 남는가"가 더 중요하다.

## 5. 확인된 장점
- 서버/애플리케이션이 같은 로깅 계약을 사용한다.
- 카테고리별 파일 분리(`outputDirectory/category/yyyyMMdd_category.log`)가 이미 동작한다.
- 최소 레벨 필터링, 콘솔 on/off, 파일 on/off, thread id 포함 여부가 모두 설정값으로 제어된다.
- `FCompositeLogger`로 다중 싱크 구성이 가능하다.

## 6. 현재 한계와 리스크

### 6-1. `FCompositeLogger`는 동적 변경에 안전하지 않음
- [FCompositeLogger.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Logging\FCompositeLogger.cpp)는 `m_sinks` 접근에 동기화가 없다.
- 현재 사용 방식은 시작 시 `AddSink()` 후 런타임에는 읽기만 하므로 문제 가능성이 낮다.
- 하지만 런타임 중 sink 추가/제거를 허용할 계획이면 별도 동기화 또는 immutable snapshot 전략이 필요하다.

### 6-2. `FFileLogger`는 파일 I/O가 완전 동기식
- [FFileLogger.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Logging\FFileLogger.cpp)는 로그마다 경로 계산, 디렉터리 확인, 파일 열기/쓰기/닫기를 수행한다.
- 지금 단계에선 단순하고 안전하지만, 로그량이 많은 서버 부하 상황에서는 비용이 커질 수 있다.
- 이후에는 파일 핸들 캐시나 전용 flush thread 도입을 검토할 수 있다.

### 6-3. 오류 전파가 약함
- 파일 열기 실패 시 현재는 조용히 return 한다.
- 운영 단계에선 파일 쓰기 실패를 대체 경로(콘솔, fallback logger, once-only stderr)로 남기는 장치가 필요하다.

## 7. 검증 근거
- 빌드:
  - [EchoServer.vcxproj](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\EchoServer.vcxproj)
  - [EchoClient.vcxproj](D:\Project\ServerPortfolio\RefactoringServer\EchoClient\EchoClient.vcxproj)
- 런타임:
  - `EchoServer.exe` 실행
  - `EchoClient.exe` 실행
  - `echo validation succeeded.` 확인
- 실제 사용 경로:
  - [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)에서 `FCompositeLogger + FConsoleLogger + FFileLogger` 조합 생성
  - [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FIocpServer.cpp)에서 시작/종료/오류/세션 이벤트 로그 호출

## 8. 다음 작업 후보
- `logging/testing-history.md` 추가 후 파일 생성/출력 검증 이력 누적
- `FNullLogger` 또는 테스트용 메모리 싱크 추가
- `FCompositeLogger` sink 구성 완료 후 immutable로 고정하는 정책 추가
- 파일 핸들 재사용 또는 async logger 2차 설계
