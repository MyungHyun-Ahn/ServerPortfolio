# Foundation Module Layout Plan

## 1. 목적
- `RefactoringServer`에서 여러 하위 모듈이 함께 사용하는 공용 기반 계층의 이름과 경계를 고정한다.
- `NetworkLib` 내부에 임시로 들어가 있는 공용 모듈을 어디로 옮길지 기준을 만든다.

## 2. 디렉터리 이름 결정
- 공용 기반 계층 루트 이름은 `Foundation`으로 고정한다.
- 이유:
  - `Common`보다 계층 의미가 더 분명하다.
  - `Utils`처럼 잡다한 유틸리티 창고로 흐를 가능성이 낮다.
  - `NetworkLib`보다 아래에 있는 공용 기반이라는 느낌을 주기 좋다.

## 3. 기본 구조 초안
- `RefactoringServer/Foundation/Logging`
- `RefactoringServer/Foundation/Diagnostics`
- 이후 필요 시:
  - `RefactoringServer/Foundation/Config`
  - `RefactoringServer/Foundation/Time`
  - `RefactoringServer/Foundation/Text`

## 4. 배치 원칙
- 특정 도메인에 종속되지 않는 모듈만 `Foundation` 아래에 둔다.
- `NetworkLib` 전용 정책이나 네트워크 프로토콜에 직접 묶인 타입은 `NetworkLib`에 둔다.
- 상위 서버, 테스트 실행기, 진단 모듈이 공통으로 필요로 하는 기능이면 `Foundation` 후보로 본다.

## 5. 현재 이동 대상
- `Logging`
  - 현재는 `Foundation/Logging`으로 이동 완료
- `CrashDump`
  - 처음부터 `Foundation/Diagnostics` 아래 공용 모듈로 두는 것이 적합하다.

## 6. 현재 비이동 대상
- `Containers`
- `Memory`
- `Servers`
  - 현재는 `NetworkLib` 코어와 결합도가 높으므로 우선 그대로 둔다.

## 7. 단계별 적용 순서
1. 문서 기준을 `Foundation` 구조로 먼저 고정
2. `Logging` 모듈을 `Foundation/Logging`으로 이동 완료
3. 새 `FCrashDump`를 `Foundation/Diagnostics`에 생성
4. 이후 필요 시 `Config`, `Time`, `Text` 같은 공용 모듈 추가 검토

## 8. 현재 결론
- 공용 기반 계층은 `Foundation`으로 부르는 것이 가장 적합하다.
- 로거와 크래시 덤프는 `NetworkLib` 내부보다 `Foundation` 아래에 두는 편이 구조상 자연스럽다.
