# ContentsRuntime lock-free inbox 토글 정책

## 1. 목적
- `packet inbox` lock-free 프로토타입을 실험하되, 문제가 생기면 즉시 기존 경로로 되돌릴 수 있게 한다.

## 2. 현재 토글 위치
- [FContentThread.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Core\FContentThread.cpp)
- 상수:
  - `kUseLockFreePacketInboxPrototype`

## 3. 현재 정책
- 기본값은 코드 상수로 관리한다.
- 이유
  - 아직 실험 단계라서 빠른 on/off가 필요하다.
  - 실패 시 가장 간단한 rollback 경로가 필요하다.

## 4. 운영 규칙
- 새 기능 검증 전에는 짧은 스모크를 먼저 돌린다.
- 장시간 검증 전에는 race injection 켠 짧은 검증을 다시 통과해야 한다.
- 문제가 나면
  1. 토글을 `false`로 변경
  2. 기존 `deque + mutex` 경로로 되돌림
  3. 원인 분석 후 재도전

## 5. 향후 방향
- 지금은 compile-time 토글로 유지한다.
- 장기적으로 안정성이 충분히 확보되면
  - build configuration
  - 또는 테스트 전용 런타임 옵션
  으로 확장할 수 있다.

## 6. 주의
- race injection 실험과 lock-free 토글 변경은 성능 결과와 안정성 결과를 함께 기록해야 한다.
- 장시간 soak 통과 전까지는 “기본 경로 완전 대체”로 간주하지 않는다.
