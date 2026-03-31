# 문서 인덱스

## 1. 기준 문서
| 구분 | 문서 | 목적 | 상태 |
| --- | --- | --- | --- |
| 전환 방향 | [2026-03-31_mmorpg-server-transition-design.md](D:\Project\ServerPortfolio\docs\design\2026-03-31_mmorpg-server-transition-design.md) | 기존 포트폴리오 서버를 MMORPG 서버 방향으로 전환하는 기준 정리 | 완료 |
| 리팩터링 기획 | [2026-03-31_networklib-game-server-refactoring-plan.md](D:\Project\ServerPortfolio\docs\design\2026-03-31_networklib-game-server-refactoring-plan.md) | `NetworkLib`를 게임 서버 코어 라이브러리로 정리하기 위한 단계별 계획 | 진행 중 |
| 완료 판단 기준 | [2026-03-31_networklib-game-server-completion-criteria.md](D:\Project\ServerPortfolio\docs\design\2026-03-31_networklib-game-server-completion-criteria.md) | 리팩터링 기획 완료 여부를 판정할 기준과 증빙 문서 목록 | 진행 중 |

## 2. 작업 현황 요약
| 작업 항목 | 관련 문서 | 현재 상태 | 다음 확인 항목 |
| --- | --- | --- | --- |
| MMORPG 전환 방향 정리 | [2026-03-31_mmorpg-server-transition-design.md](D:\Project\ServerPortfolio\docs\design\2026-03-31_mmorpg-server-transition-design.md) | 완료 | 방향 기준을 실제 프로젝트 구조에 반영 |
| `TestClient` 도입 및 기본 검증 루프 구축 | [2026-03-31_testclient-project-design.md](D:\Project\ServerPortfolio\docs\design\2026-03-31_testclient-project-design.md) | 완료 | 스트레스 시나리오 확장 여부 판단 |
| `NetworkLib` 수명주기 위험 정적 리뷰 | [2026-03-31_networklib-lifecycle-review.md](D:\Project\ServerPortfolio\docs\code-review\2026-03-31_networklib-lifecycle-review.md) | 완료 | 리뷰 항목을 실제 리팩터링 단계와 연결 |
| `NetworkLib` 1차 세션 가드 리팩터링 | [2026-03-31_networklib-session-guard-refactoring-review.md](D:\Project\ServerPortfolio\docs\code-review\2026-03-31_networklib-session-guard-refactoring-review.md) | 완료 | 후속 안정성 리팩터링 범위 확정 |
| `NetworkLib`를 게임 서버 코어로 재구성 | [2026-03-31_networklib-game-server-refactoring-plan.md](D:\Project\ServerPortfolio\docs\design\2026-03-31_networklib-game-server-refactoring-plan.md) | 진행 중 | 종료 구조, 콘텐츠 경계, 실행 프로젝트 분리 |
| MMORPG 전용 실행 서버 골격 도입 | [2026-03-31_mmorpg-server-transition-design.md](D:\Project\ServerPortfolio\docs\design\2026-03-31_mmorpg-server-transition-design.md) | 미착수 | `EchoServer` 대체 프로젝트 설계 및 생성 |

## 3. 설계 문서
| 문서 | 설명 | 상태 |
| --- | --- | --- |
| [2026-03-31_mmorpg-server-transition-design.md](D:\Project\ServerPortfolio\docs\design\2026-03-31_mmorpg-server-transition-design.md) | Echo 기반 샘플 서버에서 MMORPG 서버 방향으로 전환하는 큰 흐름 정리 | 완료 |
| [2026-03-31_networklib-game-server-refactoring-plan.md](D:\Project\ServerPortfolio\docs\design\2026-03-31_networklib-game-server-refactoring-plan.md) | `NetworkLib` 리팩터링 목표, 범위, 우선순위 정리 | 진행 중 |
| [2026-03-31_networklib-game-server-completion-criteria.md](D:\Project\ServerPortfolio\docs\design\2026-03-31_networklib-game-server-completion-criteria.md) | 리팩터링 완료 판정 기준과 단계별 증빙 항목 정리 | 진행 중 |
| [2026-03-31_testclient-project-design.md](D:\Project\ServerPortfolio\docs\design\2026-03-31_testclient-project-design.md) | 서버 검증용 `TestClient`의 목적, 재사용 자산, 테스트 범위 정리 | 완료 |

## 4. 코드 리뷰 문서
| 문서 | 설명 | 상태 |
| --- | --- | --- |
| [2026-03-31_networklib-lifecycle-review.md](D:\Project\ServerPortfolio\docs\code-review\2026-03-31_networklib-lifecycle-review.md) | 세션 수명주기, 콘텐츠 이동, 종료 흐름의 위험 지점 정리 | 완료 |
| [2026-03-31_networklib-session-guard-refactoring-review.md](D:\Project\ServerPortfolio\docs\code-review\2026-03-31_networklib-session-guard-refactoring-review.md) | 1차 세션 가드 리팩터링 근거와 런타임 테스트 결과 정리 | 완료 |

## 5. 상태 기준
| 상태 | 의미 |
| --- | --- |
| 완료 | 문서 목적에 해당하는 작업이 끝났고 근거 문서 또는 테스트가 연결된 상태 |
| 진행 중 | 방향과 일부 구현은 존재하지만 후속 작업이 남아 있는 상태 |
| 미착수 | 관련 필요성은 확인됐지만 아직 설계나 구현이 시작되지 않은 상태 |

## 6. 문서 운용 원칙
| 항목 | 원칙 |
| --- | --- |
| 설계 문서 추가 | 새 기획이나 구조 변경이 생기면 `docs/design/`에 문서를 추가하고 이 인덱스에 반영 |
| 리뷰 문서 추가 | 안정성, 수명주기, 성능 판단 근거가 필요한 변경은 `docs/code-review/`에 정리 |
| 완료 기준 갱신 | 리팩터링 단계가 바뀌면 완료 판단 문서와 이 인덱스의 상태를 함께 갱신 |
