# Legacy MHLib Reuse Plan

## 1. 목적
- `includes/MHLib`에 있는 기존 자산 중 `RefactoringServer`에서 재사용 가치가 있는 항목을 선별한다.
- 그대로 복사하는 대신, 새 코딩 컨벤션과 새 아키텍처 경계에 맞게 `재설계 후 이식`할 대상을 정한다.
- 당장 가져올 것과 나중에 참고만 할 것을 구분해 불필요한 레거시 의존 유입을 막는다.

## 2. 현재 판단 기준
- `RefactoringServer` 기준으로 새 `NetworkLib`는 최소 서버 코어와 lock-free 기반 유틸리티를 담당한다.
- 새 구조에서는 전역 singleton, 강한 매크로 의존, 기존 프로젝트 경로 결합을 피한다.
- 따라서 `MHLib` 자산은 아래 기준으로 평가한다.
  - 알고리즘이나 로직이 독립적인가
  - `RefactoringServer` 아키텍처 경계와 충돌하지 않는가
  - Windows + MSVC 서버 환경에서 실제 운영 가치가 있는가
  - 지금 단계에서 바로 필요한가

## 3. 분류 결과
### 3-1. 우선 이식 후보
- `security/CEncryption.h`
  - 패킷 인코딩/디코딩 로직 자체는 독립성이 높다.
  - 현재 구현은 `PACKET_KEY` 전역 스타일과 `windows.h` 의존이 섞여 있으므로, 새 `NetworkLib` 유틸리티로 다시 감싸는 방식이 적합하다.
- `debug/CCrashDump.h`
  - Windows/MSVC 기반 서버 포트폴리오에서 어필 포인트가 있다.
  - 다만 현재는 `CLogger` 전역 포인터에 기대므로, 새 구조에서는 로거 의존 없이 독립 초기화 가능한 형태로 바꿔야 한다.

### 3-2. 참고 자산
- `utils/CFileLoader.h`
  - 설정 파서로서 참고 가치는 있다.
  - 다만 UTF-16LE 고정, 수동 문자열 파싱, 오버로드 위주 API라서 그대로 이식하기보다 새 설정 로더 설계 참고본으로 쓰는 편이 낫다.
- `memory/CTLSPagePool.h`
  - page-locked 버퍼 캐시 아이디어는 네트워크 버퍼 풀 설계 때 다시 볼 가치가 있다.
  - 현재 복잡도와 레거시 결합이 커서 지금 바로 가져오지는 않는다.
- `utils/CProfileManager.h`
  - TLS 기반 프로파일링 개념은 유효하다.
  - 다만 전역 singleton, 매크로, TLS 직접 관리 결합이 강해 현재 단계에서는 참고만 한다.

### 3-3. 현재 구조 유지 대상
- `containers/CLFQueue.h`
- `containers/CLFStack.h`
- `memory/CLFMemoryPool.h`
- `memory/CTLSMemoryPool.h`
  - 위 자산은 이미 `RefactoringServer/NetworkLib`의 `FLockFreeQueue`, `FLockFreeStack`, `FLockFreeMemoryPool`, `FTlsMemoryPoolManager`로 재해석한 상태다.
  - 이후 비교 기준이나 아이디어 참고본으로만 유지한다.

### 3-4. 비이식 대상
- `utils/CLogger.h`
- `utils/DefineSingleton.h`
- `utils/CMonitoringManager.h`
- `utils/CMonitor.h`
- `utils/CSmartPtr.h`
- `utils/CLockGuard.h`
- `containers/CDeque.h`
  - 전역 singleton, 기존 프로젝트 결합, 현재 lock-free 코어 방향과 충돌하는 요소가 커서 직접 이식 대상에서 제외한다.

## 4. 우선 작업 순서
### 4-1. 1차
- `CCrashDump` 재설계
  - 목표
    - `RefactoringServer/NetworkLib` 또는 공용 유틸리티 계층에 맞는 독립형 크래시 덤프 초기화기 설계
    - 로거 의존 제거
    - 최소 API 정의
  - 산출물
    - 설계 문서 1개
    - 구현 초안 1개

### 4-2. 2차
- `CEncryption` 재설계
  - 목표
    - `PACKET_KEY` 전역값 제거
    - 명시적 key 전달 또는 설정 객체 기반 API로 변경
    - 패킷 프레이밍 계층과 결합 가능한 형태 정의
  - 산출물
    - 설계 문서 1개
    - 단위 검증 또는 echo 검증 보강

### 4-3. 3차
- 설정 로더 설계
  - 목표
    - `CFileLoader`를 직접 들여오지 않고, 새 설정 포맷과 로더 인터페이스 초안 작성
    - UTF-8 텍스트 기준으로 단순하고 예측 가능한 파서 방향 고정

## 5. 적용 원칙
- `includes/MHLib` 파일을 그대로 복사하지 않는다.
- 새 파일은 `RefactoringServer` 하위 새 이름과 새 규칙으로 작성한다.
- 레거시 코드는 비교 대상으로만 남기고, 구현 근거는 문서에 명시한다.
- 이식 후에는 반드시 현재 `RefactoringServer` 테스트 흐름에 맞는 검증 경로를 붙인다.

## 6. 확정된 사실
- `CEncryption`과 `CCrashDump`는 지금 기준에서도 재사용 가치가 있다.
- `CFileLoader`, `CTLSPagePool`, `CProfileManager`는 아이디어 참고 가치가 있다.
- logger, singleton, monitoring 계열은 새 구조에 바로 들이는 것이 오히려 결합도를 높일 가능성이 크다.

## 7. 다음 확인 항목
- `CCrashDump`를 `NetworkLib`에 둘지, `RefactoringServer` 공용 유틸리티 계층으로 뺄지 결정
- `CEncryption`을 패킷 계층 도입 전 미리 넣을지, 프레이밍 설계 후 같이 넣을지 결정
- 새 설정 로더가 필요한 시점을 `EchoServer` 다음 샘플 서버 도입 전으로 볼지 여부 확인
