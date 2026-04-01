# Legacy MHLib Reuse Plan

## 1. 목적
- `includes/MHLib` 자산 중 `RefactoringServer`에서 다시 쓸 가치가 있는 항목을 분류한다.
- 그대로 복사하는 대신 현재 구조와 코딩 규칙에 맞게 `재설계 후 이식`할 대상을 정한다.

## 2. 현재 기준
- `RefactoringServer`는 `NetworkLib`와 공용 기반 계층 `Foundation`을 분리하는 방향으로 간다.
- 전역 singleton, 강한 매크로 의존, 기존 프로젝트 경로 결합 코드는 피한다.

## 3. 분류 결과

### 3-1. 우선 이식 후보
- `security/CEncryption.h`
  - 패킷 인코딩/디코딩 로직 자체는 독립성이 높다.
  - 다만 전역 key와 기존 의존은 제거하고 재설계가 필요하다.
- `debug/CCrashDump.h`
  - Windows/MSVC 서버 환경에서 가치가 크다.
  - `Foundation/Diagnostics` 공용 진단 모듈로 재설계 후 이식한다.

### 3-2. 참고 자산
- `utils/CFileLoader.h`
  - 설정 로더 설계 참고본
- `memory/CTLSPagePool.h`
  - page-locked 버퍼 캐시 아이디어 참고
- `utils/CProfileManager.h`
  - TLS 기반 프로파일링 아이디어 참고

### 3-3. 이미 현재 구조로 흡수한 자산
- `containers/CLFQueue.h`
- `containers/CLFStack.h`
- `memory/CLFMemoryPool.h`
- `memory/CTLSMemoryPool.h`
  - 현재 `RefactoringServer/NetworkLib`의 lock-free 자료구조와 memory pool에 이미 재해석되어 반영됐다.

### 3-4. 직접 이식 제외 대상
- `utils/CLogger.h`
- `utils/DefineSingleton.h`
- `utils/CMonitoringManager.h`
- `utils/CMonitor.h`
- `utils/CSmartPtr.h`
- `utils/CLockGuard.h`
- `containers/CDeque.h`
  - 전역 singleton, 기존 프로젝트 결합, 현재 구조와 충돌하는 요소가 커서 직접 이식 대상에서 제외한다.

## 4. 우선 작업 순서
1. `CCrashDump` 재설계
   - 목표:
     - `Foundation/Diagnostics`용 독립 크래시 덤프 구조 설계
     - 로거 의존 제거
     - 최소 API 정의
2. `CEncryption` 재설계
   - 목표:
     - 전역 key 제거
     - 명시적 설정/주입 기반 API로 변경
3. 설정 로더 설계
   - 목표:
     - `CFileLoader`를 직접 들여오지 않고 새 로더 설계

## 5. 적용 원칙
- `MHLib` 파일을 그대로 복사하지 않는다.
- 새 파일은 `RefactoringServer` 하위 새 이름과 새 규칙으로 작성한다.
- 근거와 차이점은 문서에 남긴다.

## 6. 현재 결론
- `CCrashDump`와 `CEncryption`은 여전히 재사용 가치가 높다.
- `CLogger`는 직접 이식 대신 공용 기반 모듈 `Foundation/Logging`으로 재설계하는 것이 맞다.
- 나머지 유틸리티는 현재 단계에서는 참고 자산으로만 둔다.
