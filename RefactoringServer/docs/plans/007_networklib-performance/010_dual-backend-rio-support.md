# Dual Backend RIO Support Plan

## 1. 목적
- `NetworkLib`가 `IOCP`와 `RIO`를 모두 backend로 지원하도록 확장한다.
- 상위 프로젝트는 config의 `Backend` 값만 바꿔 backend를 선택하고, `EchoServer`, `ContentsRuntime`, packet handler는 transport 구현 세부사항을 모르게 유지한다.
- 이후 `IOCP vs RIO` 비교 벤치마크가 가능한 구조를 만든다.

## 2. 현재 결론
- 방향은 `IOCP 교체`가 아니라 `IOCP 유지 + RIO 추가`다.
- public API는 [IServer](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\IServer.h)를 유지한다.
- 내부는 backend와 session을 분리한다.
- `RIO`는 첫 단계에서 stub과 구조 분리만 완료하고, 실제 CQ/RQ 구현은 후속 단계에서 진행한다.

## 3. 1차 완료 범위
- 세션 추상 경계 추가
  - [ISession.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\ISession.h)
- IOCP 세션 분리
  - [FIocpSession.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FIocpSession.h)
  - [FIocpSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FIocpSession.cpp)
- RIO 세션 뼈대 추가
  - [FRioSession.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FRioSession.h)
  - [FRioSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FRioSession.cpp)
- RIO 서버 뼈대 추가
  - [FRioServer.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.h)
  - [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp)
- `FIocpServer`는 새 `FIocpSession`을 사용하도록 정리했다.
- [FServerFactory.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FServerFactory.cpp)에서 `Backend: Rio`면 `FRioServer`를 선택하도록 연결했다.

## 4. 구조
### 4.1 Public Layer
- [IServer](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\IServer.h)
- [IApplicationHandler](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\IApplicationHandler.h)

### 4.2 Backend Layer
- [FIocpServer](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.h)
- [FRioServer](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.h)
- [FServerFactory](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FServerFactory.h)

### 4.3 Session Layer
- [ISession](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\ISession.h)
- [FIocpSession](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FIocpSession.h)
- [FRioSession](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FRioSession.h)

## 5. IOCP 현재 상태
- `FIocpServer`의 동작 의미는 유지했다.
- 예전 `FSession`을 `FIocpSession`으로 분리했지만, 아래 흐름은 그대로다.
  - accept
  - session attach
  - `WSARecv` completion
  - recv buffer / framer / cipher
  - application dispatch
  - send queue
  - `TryBeginSend / EndSend` 기반 send 재기동
- 중간에 검토했던 `owner-worker least-loaded` 실험은 이번 범위에서 제거했다.

## 6. 1차 검증
- `Debug x64` 솔루션 빌드 성공
- `IOCP` 스모크 성공
  - `EchoServer + EchoClient`
  - `1세션` 기본 스모크 통과
- `IOCP` 회귀 검증 성공
  - `100세션`
  - `3분`
  - 성공 로그:
    - [client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\iocp_regression_100x3m\client.log)
- `RIO` 선택 경로 확인
  - 현재는 의도대로 stub 경로에 진입해 `RIO backend is not implemented yet.`를 출력하고 종료한다.
  - 확인 로그:
    - [rio_server.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_refactor_smoke\rio_server.log)

## 7. RIO 구현 다음 단계
1. `FRioServer` startup/shutdown 실제 구현
2. RIO function table 초기화와 capability check
3. registered buffer pool 설계
4. `FRioSession`에 RQ/CQ 관련 상태 추가
5. accept 이후 socket을 RIO 경로에 attach
6. send/recv submit 및 completion dequeue 구현
7. `EchoServer` smoke
8. `IOCP vs RIO` 비교 벤치마크

## 8. Lock-Free 판단
- 목표는 `완전 무락`이 아니라 `hot path near lock-free`다.
- 첫 구현은 lock을 허용해 정확성을 먼저 확보한다.
- 이후 효과가 큰 구간만 lock-free로 옮긴다.
  - send enqueue
  - completion handoff
  - buffer free-list

## 9. 현재 결론
- 지금 구조는 `RIO`를 실제로 넣기 위한 1차 분리 작업까지 끝났다.
- `IOCP` 기준선은 유지되고 있다.
- 다음 작업은 `FRioServer / FRioSession` 내부 구현이다.
