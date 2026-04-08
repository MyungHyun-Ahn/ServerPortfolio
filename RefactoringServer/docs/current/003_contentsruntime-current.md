# 003 ContentsRuntime Current

Status: Active  
Canonical: Yes  
Last Updated: 2026-04-08  
Scope: ContentsRuntime current execution model

## 1. 현재 실행 모델
- 현재 기준 구조는 `content-owned mailbox + worker executor`다.
- `content instance = dedicated thread` 구조는 현재 기준이 아니다.
- `delegate / work stealing`은 별도 복제 큐보다 `mailbox owner transfer` 중심으로 정리되어 있다.

## 2. 현재 적용 대상
- [EchoServer](D:\Project\ServerPortfolio\RefactoringServer\Echo\EchoServer)
- [ChattingServer](D:\Project\ServerPortfolio\RefactoringServer\Chatting\ChattingServer)

## 3. 현재 중요 규칙
- content 전환과 session 이동은 응답 패킷보다 앞서 정리해야 하는 구간이 있다.
- mailbox owner transfer와 move 충돌은 guard를 두고 막는 방향이 현재 기준이다.
- room/lobby 흐름 검증은 ChattingServer 시나리오로 이어서 본다.

## 4. 현재 상태
- mailbox owner transfer 기반 delegate/work stealing 구현까지는 완료된 상태로 본다.
- ContentsRuntime 자체보다, 이를 사용하는 `ChattingServer` 시나리오와 benchmark 검증이 현재 더 우선이다.

## 5. 관련 핵심 문서
- [012_mailbox-owner-transfer-work-stealing-and-delegate.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime\012_mailbox-owner-transfer-work-stealing-and-delegate.md)
- [003_content-transition-rules.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\003_content-transition-rules.md)
- [015_content-worker-pool-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\015_content-worker-pool-review.md)
- [016_delegate-work-stealing-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\016_delegate-work-stealing-review.md)
- [017_delegate-migration-backlog-trace-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\017_delegate-migration-backlog-trace-review.md)
