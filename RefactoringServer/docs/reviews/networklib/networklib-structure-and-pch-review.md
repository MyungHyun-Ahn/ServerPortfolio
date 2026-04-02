# NetworkLib Structure And PCH Review

## 1. 목적
- `NetworkLib` 디렉터리를 책임 기준으로 더 읽기 쉬운 구조로 나눈다.
- 헤더 의존성을 줄이고 공용 PCH 사용 규칙을 명확히 한다.
- 최상위 `GameServer` 네임스페이스를 제거하고 디렉터리 기준 네임스페이스로 정리한다.

## 2. 적용 내용
- `Servers/Core`
  - 서버 설정, 팩토리, IOCP 구현, placeholder 백엔드를 배치했다.
- `Servers/Session`
  - `FSession` 같은 세션 로컬 상태를 분리했다.
- `Packet`
  - 한 디렉터리에 몰아두지 않고 목적별로 다시 나눴다.
  - `Packet/Buffer`
  - `Packet/Framing`
  - `Packet/Serialization`
  - `Packet/View`
- `Servers`
  - 외부 공개 인터페이스인 `IServer`, `IApplicationHandler`만 남겼다.

## 3. Packet 구조 정리 판단
- `FRecvBuffer`와 `FSendBuffer`는 둘 다 송수신 패킷 버퍼 계층에 속하므로 `Packet/Buffer`가 가장 자연스럽다.
- `FPacketReader`, `FPacketWriter`, `FPacketSerialization`은 generated packet과 직접 맞물리므로 `Packet/Serialization`으로 분리하는 편이 읽기 쉽다.
- `FPacketView`, `FBorrowedViewGuard`는 zero-copy borrowed view 수명 문제를 다루므로 `Packet/View`가 목적을 가장 잘 드러낸다.
- `PacketTypes`, `ContentHeader`, `IPacketFramer`, `FDefaultPacketFramer`는 framing 책임이므로 `Packet/Framing`에 두는 편이 명확하다.

## 4. 네임스페이스 정리
- 기존 `GameServer::Foundation`, `GameServer::NetworkLib`, `GameServer::Generated`는 아래처럼 정리했다.
  - `Foundation`
  - `NetworkLib`
  - `Generated`
- 내부 구현 네임스페이스도 디렉터리 기준으로 맞췄다.
  - `NetworkLib::Core`
  - `NetworkLib::Session`
  - `NetworkLib::Packet::Buffer`
  - `NetworkLib::Packet::Framing`
  - `NetworkLib::Packet::Serialization`
  - `NetworkLib::Packet::View`

## 5. 헤더 / PCH 정리
- `IServer.h`, `IApplicationHandler.h`, `FServerFactory.h`는 forward declaration 중심으로 가볍게 유지한다.
- `NetworkLib` 공용 PCH 묶음은 [`NetLibPch.h`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\NetLibPch.h)에서 관리한다.
- 소비 프로젝트는 자기 `Pch.h`에서 `NetLibPch.h` 한 줄로 공용 의존성을 가져온다.
- 헤더에서 include를 기본값으로 남발하지 않고, 필요한 경우만 예외적으로 남긴다.

## 6. 현재 판단
- `Servers`와 `Packet`을 목적별로 나눈 뒤 구조 가독성이 확실히 좋아졌다.
- generated packet/handler/router까지 새 네임스페이스 구조에 맞춰 재생성 가능한 상태를 확인했다.
- `EchoServer`, `EchoClient`, `LockFreeTests` 빌드와 스모크 검증까지 통과했으므로 현재 구조는 실사용 가능한 단계다.
