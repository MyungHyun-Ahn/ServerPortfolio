# NetworkLib 개요

## 1. 역할
- `NetworkLib`는 게임 로직을 직접 실행하는 계층이 아니다.
- 비동기 네트워크 I/O, 세션 생명주기, 패킷 프레이밍/직렬화, 애플리케이션 패킷 전달 경계를 제공한다.
- 상위 계층은 `sessionId + packet` 단위로만 네트워크를 다루고, 콘텐츠 실행 모델은 `ContentsRuntime`가 맡는다.

## 2. 현재 책임
- socket 생성, accept, recv/send, completion 처리
- 세션 생성/해제와 세션 상태 관리
- transport header / content header / packet framing
- packet reader/writer, generated packet 직렬화 지원
- borrowed packet view와 zero-copy 보조 구조
- 성능 계측과 page pool / TLS pool 같은 보조 최적화

## 3. 현재 비책임
- 콘텐츠 스레드 실행 루프
- 콘텐츠 전이 규칙
- 로비, 룸, 배틀 같은 게임 규칙
- DB, Redis, 운영 명령 계층

## 4. 디렉터리 구조
- `Containers`
  - lock-free queue/stack
- `Crypto`
  - packet cipher 인터페이스와 기본 구현
- `Memory`
  - lock-free memory pool, TLS memory pool
- `Packet/Buffer`
  - [FPacketBuffer](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\Buffer\FPacketBuffer.h)
  - [FRecvBuffer](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\Buffer\FRecvBuffer.h)
  - [FSendBuffer](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\Buffer\FSendBuffer.h)
- `Packet/Framing`
  - [PacketTypes](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\Framing\PacketTypes.h)
  - [ContentHeader](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\Framing\ContentHeader.h)
  - [IPacketFramer](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\Framing\IPacketFramer.h)
  - [FDefaultPacketFramer](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\Framing\FDefaultPacketFramer.h)
- `Packet/Serialization`
  - [FPacketReader](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\Serialization\FPacketReader.h)
  - [FPacketWriter](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\Serialization\FPacketWriter.h)
  - [FPacketSerialization](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\Serialization\FPacketSerialization.h)
  - [IContentPacket](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\Serialization\IContentPacket.h)
- `Packet/View`
  - [FPacketView](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\View\FPacketView.h)
  - [FBorrowedViewGuard](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\View\FBorrowedViewGuard.h)
- `Servers/Core`
  - [BackendTypes](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\BackendTypes.h)
  - [FServerFactory](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FServerFactory.h)
  - [FIocpServer](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.h)
  - [FRioServer](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.h)
  - [FStubServer](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FStubServer.h)
- `Servers/Session`
  - [ISession](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\ISession.h)
  - [FIocpSession](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FIocpSession.h)
  - [FRioSession](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FRioSession.h)
- `Servers`
  - [IServer](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\IServer.h)
  - [IApplicationHandler](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\IApplicationHandler.h)

## 5. Backend 전략
- public 경계는 [IServer](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\IServer.h) 하나로 유지한다.
- backend 선택은 [FServerFactory](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FServerFactory.h)가 맡는다.
- 현재 backend 상태는 다음과 같다.
  - `Iocp` -> [FIocpServer](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.h)
  - `Rio` -> [FRioServer](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.h) stub
  - `BoostAsio` -> [FStubServer](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FStubServer.h)

## 6. Session 전략
- 세션도 backend별 구현으로 나눈다.
- 공통 경계는 [ISession](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\ISession.h)이다.
- `IOCP` 경로는 [FIocpSession](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FIocpSession.h)이 맡는다.
- `RIO` 경로는 [FRioSession](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FRioSession.h)이 맡는다.
- 즉 `NetworkLib`는 `single IOCP session implementation`에서 `dual-backend 확장이 가능한 구조`로 넘어간 상태다.

## 7. 상위 계층과의 경계
- `NetworkLib`는 `sessionId + packet` 전달까지만 책임진다.
- 콘텐츠 스레드, 콘텐츠 전이, 콘텐츠 플레이 루프는 `ContentsRuntime` 프로젝트가 담당한다.
- 네트워크와 게임 로직 실행 모델을 강하게 결합하지 않는 것이 현재 구조의 목표다.

## 8. 헤더 / PCH 규칙
- 공용 인터페이스 헤더는 forward declaration을 우선 사용한다.
- 자주 바뀌지 않는 공용 의존성은 [NetLibPch.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\NetLibPch.h)로 모은다.
- 자세한 규칙은 [003_cpp-header-pch-convention.md](D:\Project\ServerPortfolio\RefactoringServer\docs\architecture\001_project\003_cpp-header-pch-convention.md)를 따른다.
