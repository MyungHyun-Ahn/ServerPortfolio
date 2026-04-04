# Dual Backend Session/Server Split Review

## 1. 목적
- `NetworkLib`가 `IOCP`와 `RIO`를 모두 backend로 지원할 수 있도록 내부 구조를 분리한 작업을 정리한다.
- 이번 단계의 목표는 `RIO 완성`이 아니라, `IOCP 동작 유지 + RIO 진입점 확보`다.

## 2. 핵심 변경
### 2.1 Session 분리
- 기존 단일 구현:
  - `FSession`
- 현재 구조:
  - [ISession](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\ISession.h)
  - [FIocpSession](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FIocpSession.h)
  - [FRioSession](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FRioSession.h)

`ISession`은 최소 공통 수명주기와 통계만 가진다.
- `sessionId`
- `slotIndex`
- `generation`
- closing / refcount
- queued send count

IOCP 전용 상태는 `FIocpSession`으로 내렸다.
- `OVERLAPPED`
- `WSABUF`
- recv buffer
- send queue
- `TryBeginSend / EndSend`

RIO 전용 상태는 `FRioSession`으로 둘 자리를 먼저 만들었다.
- 현재는 skeleton만 있고 실제 `RQ/CQ` 상태는 후속 구현 범위다.

### 2.2 Server 분리
- 기존:
  - [FIocpServer](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.h)
- 추가:
  - [FRioServer](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.h)

현재 의미:
- `FIocpServer`는 실제 동작 backend
- `FRioServer`는 backend entry stub

### 2.3 Factory 연결
- [FServerFactory.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FServerFactory.cpp)
- `Backend: Iocp` -> `FIocpServer`
- `Backend: Rio` -> `FRioServer`

## 3. IOCP 회귀 여부
이번 분리 작업에서 가장 중요한 조건은 `IOCP 동작 보존`이었다.

확인한 내용:
- 솔루션 `Debug x64` 빌드 성공
- `EchoServer + EchoClient` 기본 스모크 성공
- `IOCP 100세션 / 3분` 회귀 성공

검증 로그:
- [client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\iocp_regression_100x3m\client.log)

결과:
- `echo validation succeeded. sessions=100 responses=17912 ... holdSeconds=180`
- 클라이언트 에러 로그 비어 있음
- 서버 에러 로그 비어 있음

## 4. RIO 현재 상태
현재 `RIO`는 “선택 가능하지만 미구현” 상태다.

확인한 내용:
- `Backend: Rio`로 서버를 기동하면 [FRioServer](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.h)가 선택된다.
- 현재는 명시적으로 `RIO backend is not implemented yet.`를 출력하고 실패 종료한다.

검증 로그:
- [rio_server.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_refactor_smoke\rio_server.log)

이 상태의 장점:
- factory, config, public API 경로가 이미 `RIO`를 인식한다.
- 다음 작업은 내부 구현만 채우면 된다.

## 5. 이번 단계에서 하지 않은 것
- `FRioServer` 실제 send/recv 구현
- `FRioSession`의 RQ/CQ 상태 구현
- owner-worker 정책
- session 수 기반 least-loaded 배정
- CQ ownership 정책

이 항목들은 `RIO` 실제 구현 단계에서 다시 다룬다.

## 6. 결론
- 이번 작업으로 `NetworkLib`는 `single IOCP implementation`에서 `dual-backend로 확장 가능한 구조`로 넘어갔다.
- `IOCP` 기준선은 유지됐다.
- 이제 `RIO`는 설계 문서만 있는 상태가 아니라, 실제 코드 경계와 backend 선택 경로를 가진 상태다.
