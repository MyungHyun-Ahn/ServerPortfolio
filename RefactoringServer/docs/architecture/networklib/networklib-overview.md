# NetworkLib Overview

## 1. 역할
- `NetworkLib`는 게임 로직을 직접 담는 모듈이 아니라 비동기 소켓 처리, 세션 수명주기, 패킷 프레이밍/직렬화, 콘텐츠 패킷 디스패치 기반을 제공하는 라이브러리다.
- 현재 목표는 `세션 수명주기`, `송수신 경계`, `패킷 암복호화`, `콘텐츠 패킷 생성/라우팅`, `성능 계측`을 안정적으로 제공하는 것이다.

## 2. 디렉터리 구조
- `Containers`
  - lock-free queue/stack 같은 범용 컨테이너
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

## 3. 네임스페이스 기준
- 최상위 `GameServer` 네임스페이스는 제거했다.
- 현재 최상위 네임스페이스는 아래 세 개다.
  - `Foundation`
  - `NetworkLib`
  - `Generated`
- `NetworkLib` 내부는 디렉터리 구조와 맞춰 아래처럼 나눈다.
  - `NetworkLib::Containers`
  - `NetworkLib::Crypto`
  - `NetworkLib::Memory`
  - `NetworkLib::Packet::Buffer`
  - `NetworkLib::Packet::Framing`
  - `NetworkLib::Packet::Serialization`
  - `NetworkLib::Packet::View`
  - `NetworkLib::Core`
  - `NetworkLib::Session`

## 4. 구조 분리 기준
- `Servers/Core`
  - 서버 구동, 백엔드 선택, IOCP 구현, placeholder 백엔드
- `Servers/Session`
  - 세션 객체와 세션 로컬 상태
- `Packet/Buffer`
  - 송수신 버퍼와 page/TLS 풀 재사용 대상
- `Packet/Framing`
  - transport header, content header, framed packet 구성/해석
- `Packet/Serialization`
  - 콘텐츠 패킷 직렬화/역직렬화, generated packet 기반 유틸리티
- `Packet/View`
  - zero-copy view와 borrowed view 수명 보호
- `Servers`
  - 외부에서 보는 최소 공개 인터페이스

## 5. 헤더 / PCH 규칙
- 공용 인터페이스 헤더는 forward declaration 우선이다.
- 자주 바뀌지 않는 Windows/STL 의존성과 공용 `NetworkLib` 묶음은 [`NetLibPch.h`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\NetLibPch.h)로 관리한다.
- 세부 규칙은 [cpp-header-pch-convention.md](D:\Project\ServerPortfolio\RefactoringServer\docs\architecture\project\cpp-header-pch-convention.md)를 따른다.
