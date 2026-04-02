# NetworkLib 개요

## 1. 역할
- `NetworkLib`는 게임 로직을 직접 담는 모듈이 아니다.
- 역할은 비동기 네트워크 I/O, 세션 수명주기, 패킷 프레이밍/직렬화, 콘텐츠 패킷 전달 기반을 제공하는 것이다.
- 현재 목표는 `작게 검증 가능한 네트워크 코어`를 안정적으로 유지하고, 상위 계층이 이 코어 위에 콘텐츠 런타임을 얹을 수 있게 하는 것이다.

## 2. 현재 책임
- 소켓 생성, accept, recv/send, IOCP completion 처리
- 세션 생성/해제와 상태 관리
- transport header, content header, packet framing
- packet reader/writer, generated packet 직렬화 지원
- zero-copy view(`string_view`, `bytes_view`)와 borrowed view guard
- 네트워크 계측, page pool/TLS pool 같은 성능 보조 기능

## 3. 현재 비책임
- 콘텐츠 프레임 루프
- 콘텐츠 간 세션 이동 정책
- 로비/룸/월드 같은 게임 규칙
- DB, Redis, 타이머, 운영 명령 계층

## 4. 디렉터리 구조
- `Containers`
  - lock-free queue/stack 같은 범용 자료구조
- `Crypto`
  - packet cipher 인터페이스와 기본 구현
- `Memory`
  - lock-free memory pool, TLS memory pool
- `Packet/Buffer`
  - [`FPacketBuffer`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\Buffer\FPacketBuffer.h)
  - [`FRecvBuffer`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\Buffer\FRecvBuffer.h)
  - [`FSendBuffer`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\Buffer\FSendBuffer.h)
- `Packet/Framing`
  - [`PacketTypes`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\Framing\PacketTypes.h)
  - [`ContentHeader`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\Framing\ContentHeader.h)
  - [`IPacketFramer`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\Framing\IPacketFramer.h)
  - [`FDefaultPacketFramer`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\Framing\FDefaultPacketFramer.h)
- `Packet/Serialization`
  - [`FPacketReader`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\Serialization\FPacketReader.h)
  - [`FPacketWriter`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\Serialization\FPacketWriter.h)
  - [`FPacketSerialization`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\Serialization\FPacketSerialization.h)
  - [`IContentPacket`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\Serialization\IContentPacket.h)
- `Packet/View`
  - [`FPacketView`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\View\FPacketView.h)
  - [`FBorrowedViewGuard`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\View\FBorrowedViewGuard.h)
- `Servers/Core`
  - [`BackendTypes`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\BackendTypes.h)
  - [`FServerFactory`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FServerFactory.h)
  - [`FIocpServer`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.h)
  - [`FStubServer`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FStubServer.h)
- `Servers/Session`
  - [`FSession`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FSession.h)
- `Servers`
  - [`IServer`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\IServer.h)
  - [`IApplicationHandler`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\IApplicationHandler.h)

## 5. 네임스페이스 기준
- 최상위 `GameServer` 네임스페이스는 제거했다.
- 현재 최상위 네임스페이스는 아래 세 개만 유지한다.
  - `Foundation`
  - `NetworkLib`
  - `Generated`
- `NetworkLib` 내부는 디렉터리 기준으로 다시 나눈다.
  - `NetworkLib::Containers`
  - `NetworkLib::Crypto`
  - `NetworkLib::Memory`
  - `NetworkLib::Packet::Buffer`
  - `NetworkLib::Packet::Framing`
  - `NetworkLib::Packet::Serialization`
  - `NetworkLib::Packet::View`
  - `NetworkLib::Core`
  - `NetworkLib::Session`

## 6. 상위 계층과의 경계
- `NetworkLib`는 `sessionId + packet` 전달까지만 책임진다.
- 콘텐츠 스레드, 콘텐츠 전이, 콘텐츠 프레임 루프는 `ContentsRuntime` 프로젝트가 맡는다.
- 즉 네트워크와 게임 로직 실행 모델을 강하게 결합하지 않는 것이 현재 구조의 핵심이다.

## 7. 헤더 / PCH 규칙
- 공용 인터페이스 헤더는 forward declaration을 우선한다.
- 자주 바뀌지 않는 공용 의존성은 [`NetLibPch.h`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\NetLibPch.h)로 모은다.
- 자세한 규칙은 [003_cpp-header-pch-convention.md](D:\Project\ServerPortfolio\RefactoringServer\docs\architecture\001_project\003_cpp-header-pch-convention.md)를 따른다.

## 8. 다음 관점
- `NetworkLib`는 이제 서버 기능을 얹을 수 있을 정도의 코어는 갖췄다.
- 다음 핵심 관심사는 네트워크 코어 자체보다, 이 코어를 사용하는 `ContentsRuntime`과 상위 서버 구성이다.
