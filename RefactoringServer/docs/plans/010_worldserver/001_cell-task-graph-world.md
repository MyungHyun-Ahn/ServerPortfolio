# Cell Task Graph World Architecture

## 1. 목적
- 향후 `WorldServer`를 도입할 때, cell 기반 월드 시뮬레이션을 `ContentsRuntime` worker 위에서 병렬 실행하는 방향을 정리한다.
- 현재 단계에서는 구현하지 않고, 후순위 설계 메모로만 유지한다.

## 2. 우선순위
- 현재 우선순위는 `매우 낮음`이다.
- 먼저 `ContentsRuntime` worker pool 안정화, `NetworkLib` 성능 정리, 실제 월드 콘텐츠 요구사항 확정이 선행되어야 한다.

## 3. 기본 방향
- `ContentsRuntime`는 worker thread/executor를 제공한다.
- 실제 task graph는 `WorldContent` 또는 `RegionContent` 같은 상위 content가 소유한다.
- 즉 `ContentsRuntime`가 월드 규칙을 아는 것이 아니라, `Contents`가 자기 task graph를 관리하고 `ContentsRuntime` worker를 실행 자원으로 사용한다.

## 4. 셀 기반 실행 모델
- `Cell`은 논리적 시뮬레이션 단위다.
- 각 cell은 고정 owner worker를 가지거나, phase별 task로 분해되어 실행된다.
- lock-free에 가깝게 가려면 shared world state를 즉시 수정하지 않고 `read -> local result -> commit` 구조를 써야 한다.

예시:
1. 입력/명령 수집
2. 의존 없는 cell group 병렬 처리
3. cell 간 영향 commit
4. 마지막 전역 작업 1회 처리

## 5. 3x3 주변 셀 의존
- 셀이 주변 3x3 범위를 참조하면, 인접 셀을 동시에 수정하지 않도록 phase를 나눠야 한다.
- 보통 `4-color` 계열 분할이 기본 후보가 된다.
- 같은 phase에 속한 cell은 서로 직접 의존하지 않으므로 병렬 실행 가능하다.

## 6. 권장 구조
### 6-1. ContentsRuntime 책임
- worker thread 관리
- 일회성 task enqueue
- phase barrier / completion wait 같은 공용 실행기 기능

### 6-2. WorldContent 책임
- cell graph 생성
- phase 순서 정의
- task 입력/출력 버퍼 관리
- 마지막 commit / global aggregation

즉:
- `ContentsRuntime`는 executor
- `WorldContent`는 scheduler + graph owner

## 7. 왜 Cell을 바로 Content로 두지 않나
- cell 수가 많아지면 `content instance` 수가 지나치게 커질 수 있다.
- session route, content registry, instance lifecycle 비용이 cell 개수만큼 늘어난다.
- 그래서 1차 후보는 `Cell = Content`보다 `WorldContent 내부 Cell task graph`가 더 유력하다.

## 8. 필요 기능
향후 필요할 수 있는 공용 기능:
- one-shot task dispatch
- phase dispatch
- barrier wait
- commit phase callback
- owner worker hint

이 기능은 `ContentsRuntime` 공용 실행기 수준에서 제공하고, 실제 graph 정책은 `WorldContent`가 갖는 쪽이 바람직하다.

## 9. 지금 당장 하지 않는 이유
- 현재 `ContentsRuntime`는 worker pool 1차 전환을 막 끝낸 상태다.
- 월드/셀 task graph는 실제 월드 콘텐츠 요구사항이 정리된 후 설계하는 편이 맞다.
- 따라서 지금은 구현하지 않고 `WorldServer` 후속 확장 항목으로만 남긴다.
