# Crash Dump Redesign Plan

## 1. 목적
- `includes/MHLib/debug/CCrashDump.h`의 기능적 가치를 유지하면서, `RefactoringServer` 구조에 맞는 새 크래시 덤프 모듈을 설계한다.
- 기존 구현의 전역 로거 결합, 헤더 단일 구현, 전역 static 초기화 의존을 제거한다.
- `RefactoringServer/Foundation/Diagnostics` 아래 공용 진단 모듈로 재구성한다.

## 2. 기존 구현에서 확인한 사실
- 기존 [CCrashDump.h](D:\Project\ServerPortfolio\includes\MHLib\debug\CCrashDump.h)는 다음 역할을 수행한다.
  - `SetUnhandledExceptionFilter` 기반 비정상 종료 덤프 생성
  - CRT invalid parameter, report hook, purecall handler 재지정
  - `MiniDumpWriteDump` 호출
- 기존 구현은 아래와 강하게 결합되어 있다.
  - `MHLib::utils::g_Logger` 전역 로거
  - 헤더 내부 static 멤버와 전역 인스턴스
  - `CRASH_DUMP_ON` 매크로

## 3. 왜 재설계가 필요한가
- 현재 `RefactoringServer`는 전역 singleton보다 명시적 초기화와 주입을 우선한다.
- 크래시 덤프는 로거가 없어도 동작해야 한다.
- 진단 모듈이 `NetworkLib` 내부에 있으면 공용 운영 기능이 네트워크 코어에 묶인다.

## 4. 1차 설계 목표
- `RefactoringServer/Foundation/Diagnostics` 아래 공용 진단 모듈로 둔다.
- 인터페이스와 구현을 분리한다.
- 로거가 없어도 최소 기능은 동작해야 한다.
- 로거가 있으면 부가 로그를 남길 수 있어야 한다.
- 덤프 생성 경로와 파일명 정책을 설정으로 제어할 수 있어야 한다.

## 5. 제안 구조

### 5-1. 파일 배치 초안
- `RefactoringServer/Foundation/Diagnostics/FCrashDump.h`
- `RefactoringServer/Foundation/Diagnostics/FCrashDump.cpp`
- `RefactoringServer/Foundation/Diagnostics/CrashDumpTypes.h`

### 5-2. 공개 API 초안
- `Initialize(const SCrashDumpConfig& config)`
- `Shutdown()`
- `WriteManualDumpForDiagnostics()`
- `IsInitialized()`

### 5-3. 설정 타입 초안
- `enabled`
- `outputDirectory`
- `dumpType`
- `allowOnlySingleDump`
- `logger`
  - 선택적 포인터 또는 `std::shared_ptr<ILogger>`
  - 없어도 동작 가능해야 함

## 6. manual dump API 공개 정책
- manual dump API는 제한적 외부 공개로 둔다.
- 의미:
  - 테스트 코드, 관리자 명령, 개발용 진단 경로에서는 호출 가능
  - 일반 게임 로직에서 일상적으로 호출하는 API로는 사용하지 않음
- 함수명은 `WriteManualDumpForDiagnostics()`처럼 의도가 분명한 형태로 둔다.

## 7. 1차 구현 범위
- `SetUnhandledExceptionFilter` 기반 미처리 예외 덤프
- `invalid_parameter_handler`, `purecall_handler`, `_CrtSetReportHook` 연결
- `MiniDumpWriteDump` 호출
- 출력 경로 생성
- 선택적 로거 연동
- 제한적 외부 공개 manual dump API

## 8. 1차 구현에서 제외할 것
- 비동기 업로드
- crash report 압축/전송
- symbol server 연동
- pretty stack trace
- 운영 대시보드 전송

## 9. 검증 계획
- 빌드 검증
- 수동 강제 덤프 경로 검증
  - `WriteManualDumpForDiagnostics()` 또는 테스트용 강제 예외 함수
- 덤프 파일 생성 여부 확인
- 로거 연결 시 생성 시도/성공/실패 로그 확인
- 로거 미연결 상태에서도 덤프 생성 가능 여부 확인

## 10. 현재 결론
- `CCrashDump`는 기능적 가치가 있어 재사용 후보가 맞다.
- 하지만 기존 코드는 현재 구조와 결합 방식이 맞지 않으므로 직접 이식보다 재설계가 필요하다.
- `Foundation/Diagnostics` 공용 진단 모듈로 분리하고, 덤프 설정 타입도 별도 진단 타입으로 독립시키는 방향이 현재 기준이다.
