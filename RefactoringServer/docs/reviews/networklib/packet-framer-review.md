# Packet Framer Review

## 1. 문서 목적
- `RefactoringServer/NetworkLib/Packet` 1차 프레이밍 구현의 구조와 적용 범위를 정리한다.
- `FIocpServer` recv/send 경계에 프레이머와 cipher를 연결한 근거를 남긴다.

## 2. 현재 구현 범위
- 타입:
  - [PacketTypes.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\PacketTypes.h)
- 인터페이스:
  - [IPacketFramer.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\IPacketFramer.h)
- 기본 구현:
  - [FDefaultPacketFramer.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FDefaultPacketFramer.h)
  - [FDefaultPacketFramer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FDefaultPacketFramer.cpp)
- 코어 연결:
  - [BackendTypes.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\BackendTypes.h)
  - [FIocpServer.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FIocpServer.h)
  - [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FIocpServer.cpp)

## 3. 구조 판단 근거

### 3-1. 최소 헤더를 먼저 둔 이유
- 현재 1차 헤더는 `payloadLength`, `randomKey`, `flags`만 가진다.
- 이 정도면 cipher 연결과 프레임 경계 검증에 필요한 최소 정보는 담을 수 있다.
- 패킷 타입, 체크섬, 시퀀스는 이후 프로토콜 설계 단계에서 확장하는 편이 안전하다.

### 3-2. `FIocpServer`에 세션별 누적 버퍼를 둔 이유
- TCP는 메시지 경계를 보장하지 않으므로 `recv()` 한 번이 패킷 하나라는 보장이 없다.
- 따라서 [FIocpServer.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FIocpServer.h)의 `SSessionContext`에 `recvBuffer`를 두고, `TryExtractPacket()`이 성공할 때까지 누적하는 방식으로 갔다.

### 3-3. recv 처리 순서를 고정한 이유
- 현재 recv 경계는 아래 순서로 처리한다.
  1. 수신 바이트를 세션 누적 버퍼에 추가
  2. `IPacketFramer`로 프레임 추출 시도
  3. 추출 성공 시 payload를 `IPacketCipher`로 복호화
  4. 평문 payload를 application에 전달
- 이 순서 덕분에 cipher가 바이트 스트림 전체가 아니라 “패킷 단위 payload”만 다루게 된다.

### 3-4. send 처리 순서를 고정한 이유
- 현재 send 경계는 아래 순서로 처리한다.
  1. application이 넘긴 평문 payload 준비
  2. `IPacketCipher`가 있으면 payload 암호화
  3. `IPacketFramer`가 헤더를 붙여 송신 버퍼 생성
  4. `WSASend`
- 응용 계층은 평문 payload만 다루고, 프레이밍/암호화는 `NetworkLib` 경계로 내려간다.

## 4. 현재 적용 상태
- `SServerConfig`에 아래 주입 지점이 생겼다.
  - `packetCipher`
  - `packetFramer`
- `EchoServer`는 이제 응용 계층에서 직접 암복호화를 하지 않는다.
- `EchoClient`는 같은 프레이머와 cipher를 사용해 `NetworkLib` 코어 경계와 호환되는 패킷을 생성한다.

## 5. 검증 근거
- 빌드:
  - [NetworkLib.vcxproj](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\NetworkLib.vcxproj)
  - [EchoServer.vcxproj](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\EchoServer.vcxproj)
  - [EchoClient.vcxproj](D:\Project\ServerPortfolio\RefactoringServer\EchoClient\EchoClient.vcxproj)
  - [LockFreeTests.vcxproj](D:\Project\ServerPortfolio\RefactoringServer\LockFreeTests\LockFreeTests.vcxproj)
- 테스트:
  - [LockFreeTests.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\LockFreeTests.exe)
  - [EchoServer.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\EchoServer.exe)
  - [EchoClient.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\EchoClient.exe)
- 확인된 결과:
  - `Packet framer round trip`
  - `Packet framer partial receive`
  - 암호화된 `echo-test` 요청/응답 왕복 성공

## 6. 현재 한계와 리스크
- 아직 packet header에 message type, sequence, checksum은 없다.
- `EchoClient`는 단순 예제라 단일 `recv()`로 프레임 하나가 온다고 가정한다.
- `FIocpServer`는 현재 기본 프레이머 하나만 기준으로 동작하고, 프레이머 교체 정책 테스트는 아직 없다.

## 7. 다음 작업 후보
- packet header 확장 여부 결정
- `EchoClient`도 다중 프레임/분할 수신에 대응하도록 개선
- 프레이머와 cipher 선택 정책을 서버 설정 계층에서 더 명시적으로 정리
