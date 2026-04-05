# IOCP AcceptEx 전환 정리

## 1. 목적
- `FIocpServer`의 기존 `accept()` 기반 accept path를 레거시 기준에 맞춰 `AcceptEx + IOCP completion` 구조로 전환한다.
- 전환 후에도 기존 `recv/send`, `ContentsRuntime`, `EchoServer` 흐름은 유지한다.
- 전환 결과와 남은 판단 사항을 함께 정리한다.

## 2. 현재 상태
- 상태: 완료
- 코드
  - [FIocpServer.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.h)
  - [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.cpp)
- 검증
  - `1 session / 5 sec` 스모크 성공
  - `100 sessions / 3 min` 회귀 성공
  - 고빈도 재접속 stress 검증 성공

## 3. 구현 결과
### 3-1. accept thread 제거
- 별도 blocking `accept()` thread를 제거했다.
- listen socket accept도 worker의 `GetQueuedCompletionStatus()` completion plane에서 처리한다.

### 3-2. AcceptEx 다중 pre-post
- `LoadAcceptExFunctions()`로 `AcceptEx`, `GetAcceptExSockaddrs`를 로드한다.
- `InitializeAcceptContexts()`에서 고정 길이 `SAcceptContext[]`를 만들고 각 slot에 `PostAccept(slotIndex)`를 걸어둔다.
- accept completion이 오면 `HandleAcceptCompletion(...)` 이후 같은 slot에 다시 `PostAccept(slotIndex)`를 건다.

### 3-3. attach 순서
- accept completion 후 attach 순서는 다음과 같다.
1. `SO_UPDATE_ACCEPT_CONTEXT`
2. `SO_SNDBUF` 옵션 적용 필요 시 적용
3. free session slot 탐색
4. `FIocpSession::Create()`
5. `FIocpSession::Initialize(...)`
6. `CreateIoCompletionPort(clientSocket, m_iocpHandle, ...)`
7. `OnClientConnected(sessionId)`
8. `PostRecv(*sessionContext)`

### 3-4. 실패 복구
- attach 실패 시 accepted socket을 닫고 같은 slot에 다시 `AcceptEx`를 repost 한다.
- `AcceptEx completion failed`, `AcceptEx repost failed`, `SO_UPDATE_ACCEPT_CONTEXT failed`를 별도 로그로 남긴다.

## 4. 레거시와 맞춘 의미
- 완전히 같은 메모리 구조를 옮긴 것은 아니다.
- 대신 레거시에서 중요했던 의미는 맞췄다.
  - accept를 completion plane에서 처리
  - pending accept를 여러 개 미리 게시
  - accept 완료 직후 바로 다음 accept 재게시
  - attach 전에 `SO_UPDATE_ACCEPT_CONTEXT` 적용

## 5. 현재 accept slot pool 상태
### 5-1. 적용된 것
- 현재 `FIocpServer`에는 `SAcceptContext[]` 기반 accept slot pool이 있다.
- slot에는 다음 정보가 들어간다.
  - `OVERLAPPED`
  - `acceptedSocket`
  - `slotIndex`
  - accept address buffer

### 5-2. 아직 적용되지 않은 것
- `acceptedSocket` 자체는 재사용하지 않는다.
- `PostAccept()` 호출마다 이전 socket을 닫고 새 `WSASocketW(...)`를 만든다.
- 즉 현재 구조는 `accept context slot pool`이지, `accepted socket reuse pool`은 아니다.

## 6. 재접속 stress 검증 결과
- 상세 리뷰: [017_iocp-acceptex-reconnect-stress-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\017_iocp-acceptex-reconnect-stress-review.md)
- 핵심 결과
  - `10 sessions / 3 min / reconnect 100% / delay 250ms` 성공
  - 서버 측 `AcceptEx completion failed`, `AcceptEx repost failed`, `SO_UPDATE_ACCEPT_CONTEXT failed`, `All session slots are in use` 모두 `0건`
  - `Client connected | Session closed` 합계 `20100`
- 더 공격적인 조건에서 먼저 터진 것은 `AcceptEx` 서버 경로가 아니라
  - room 수용량 부족
  - 로컬 클라이언트 ephemeral port 한계(`10048`)
였다.

## 7. socket 재사용 판단
- `accepted socket reuse`는 현재 기본 적용하지 않는다.
- 이유
  - 실측 기준 차이가 크지 않았다.
  - accept 이후 병목은 보통 socket 생성/파괴보다 attach, 첫 recv, bootstrap, send 쪽에서 더 크게 나타난다.
  - socket 재사용은 상태 초기화, 미완료 I/O 정리, attach 순서 관리가 까다롭다.
- 현재 판단
  - 기본값으로는 비활성 유지
  - 필요 시 별도 실험 옵션으로만 재검토

## 8. 남은 후속 작업
- `IOCP AcceptEx` 전환 후 accept path 성능 비교
- 필요 시 `accepted socket reuse` 실험 옵션 재도입
- 필요 시 `AcceptExCount`를 config로 노출
