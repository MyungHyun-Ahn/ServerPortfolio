# Crash Dump Module Review

## 1. 문서 목적
- `RefactoringServer/Foundation/Diagnostics` 아래 1차 크래시 덤프 모듈 구현 결과를 정리한다.
- 기존 `MHLib`의 `CCrashDump`에서 무엇을 가져오고 무엇을 버렸는지 근거를 남긴다.
- 현재 구현의 확인된 동작 범위와 남은 리스크를 분리해서 기록한다.

## 2. 현재 구현 범위
- 공개 타입:
  - [CrashDumpTypes.h](D:\Project\ServerPortfolio\RefactoringServer\Foundation\Diagnostics\CrashDumpTypes.h)
- 공개 API:
  - [FCrashDump.h](D:\Project\ServerPortfolio\RefactoringServer\Foundation\Diagnostics\FCrashDump.h)
- 구현:
  - [FCrashDump.cpp](D:\Project\ServerPortfolio\RefactoringServer\Foundation\Diagnostics\FCrashDump.cpp)
- 연결 지점:
  - [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)

## 3. 구조 판단 근거

### 3-1. `NetworkLib` 내부가 아니라 `Foundation/Diagnostics`에 둔 이유
- 크래시 덤프는 네트워크 코어 기능이 아니라 공용 진단 모듈이다.
- 이후 `NetworkLib`, `EchoServer`, 차후의 `WorldServer`, 테스트 실행기까지 같은 진단 경로를 공유할 가능성이 크다.
- 로거가 이미 [Foundation](D:\Project\ServerPortfolio\RefactoringServer\Foundation)로 이동한 상태라, 덤프 모듈도 같은 공용 계층에 두는 편이 의존 방향이 자연스럽다.

### 3-2. 설정 타입을 별도 진단 타입으로 분리한 이유
- 서버 백엔드 설정과 덤프 설정은 변경 주기와 책임이 다르다.
- 현재 [SCrashDumpConfig](D:\Project\ServerPortfolio\RefactoringServer\Foundation\Diagnostics\CrashDumpTypes.h)는 출력 경로, dump 종류, handler 설치 여부, 단일 dump 제한 여부를 진단 맥락에서만 다룬다.
- 이 분리 덕분에 `BackendTypes`에 운영 진단 옵션이 계속 섞이는 문제를 피할 수 있다.

### 3-3. manual dump API를 제한적 외부 공개로 둔 이유
- [WriteManualDumpForDiagnostics()](D:\Project\ServerPortfolio\RefactoringServer\Foundation\Diagnostics\FCrashDump.h)는 실제 예외 없이도 dump 경로를 검증할 수 있게 해준다.
- 이 API는 테스트 코드, 운영 명령, 개발 진단 경로에서 유용하다.
- 반면 일반 게임 로직에서 상시 호출할 API는 아니므로, 함수명 자체에 진단 용도임을 드러내도록 했다.

### 3-4. 전역 로거 결합을 제거한 이유
- 기존 `MHLib` 버전은 전역 로거 의존이 강했다.
- 현재 구현은 [SCrashDumpConfig](D:\Project\ServerPortfolio\RefactoringServer\Foundation\Diagnostics\CrashDumpTypes.h)의 `std::shared_ptr<ILogger>`를 선택적으로 주입받는다.
- 로거가 없을 때도 `OutputDebugStringA`로 최소 진단 경로는 유지한다.

## 4. 확인된 동작
- `SetUnhandledExceptionFilter`
- `_set_invalid_parameter_handler`
- `_set_purecall_handler`
- `_CrtSetReportHook`
- `MiniDumpWriteDump`
- 수동 dump 생성 API
- dump 중복 생성 제한

## 5. 검증 근거
- 빌드:
  - [EchoServer.vcxproj](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\EchoServer.vcxproj)
  - [EchoClient.vcxproj](D:\Project\ServerPortfolio\RefactoringServer\EchoClient\EchoClient.vcxproj)
- 수동 dump 검증:
  - `EchoServer.exe --manual-dump`
  - 실제 dump 파일 생성 확인
- 일반 런타임 검증:
  - `EchoServer.exe`
  - `EchoClient.exe`
  - `echo validation succeeded.` 확인

## 6. 현재 구현에서 확인된 장점
- 로거와의 결합은 남기되, 로거 부재 시에도 최소 동작이 가능하다.
- dump 출력 경로와 dump 종류를 설정으로 제어할 수 있다.
- `EchoServer`에서 수동 dump 경로를 바로 검증할 수 있어 초기 회귀 확인이 쉽다.
- 실행 파일 기준 경로를 사용하도록 해 로그와 dump가 작업 디렉터리에 흩어지지 않게 정리했다.

## 7. 남은 리스크와 한계

### 7-1. 비동기 업로드나 후처리는 없다
- 현재는 로컬 파일 dump 생성만 담당한다.
- symbol 정리, 업로드, 리포트 서버 전송은 2차 범위다.

### 7-2. handler 내부 정책은 최소형이다
- invalid parameter, CRT report, purecall은 현재 예외를 발생시켜 일반 dump 경로로 합류한다.
- 정책 자체는 단순하지만, 세부 분류나 추가 문맥 기록은 아직 없다.

### 7-3. 단일 프로세스 기준 검증만 끝났다
- 현재는 `EchoServer` 단독 기준으로 manual dump와 기본 왕복만 검증했다.
- 이후 `WorldServer`나 다른 실행기에도 같은 초기화/종료 패턴을 적용해 재검증할 필요가 있다.

## 8. 다음 작업 후보
- `Foundation/Diagnostics`에 `CrashContext` 또는 dump 파일명 정책 helper 분리
- 운영 명령 또는 디버그 핫키에서 manual dump API를 호출하는 경로 추가
- dump 생성 전 마지막 로그 flush 지점 보강
