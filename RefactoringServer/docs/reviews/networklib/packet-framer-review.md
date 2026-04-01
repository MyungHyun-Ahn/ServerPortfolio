# Packet Framer Review

## 1. 문서 목적
- `NetworkLib` 패킷 프레이밍 계층이 현재 어떤 헤더 구조를 사용하고 있는지 정리한다.
- 레거시 프로젝트의 패킷 구조에서 무엇을 계승했고 무엇을 버렸는지 근거를 남긴다.
- `FIocpServer` 송수신 경계에 프레이머와 cipher를 연결한 결과를 검토한다.

## 2. 현재 구현 범위
- 패킷 타입 정의:
  - [PacketTypes.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\PacketTypes.h)
- 프레이머 인터페이스:
  - [IPacketFramer.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\IPacketFramer.h)
- 기본 구현:
  - [FDefaultPacketFramer.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FDefaultPacketFramer.h)
  - [FDefaultPacketFramer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FDefaultPacketFramer.cpp)
- 코어 연결:
  - [BackendTypes.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\BackendTypes.h)
  - [IServer.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\IServer.h)
  - [IApplicationHandler.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\IApplicationHandler.h)
  - [FIocpServer.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FIocpServer.h)
  - [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FIocpServer.cpp)

## 3. 헤더 설계 근거

### 3-1. 레거시 구조에서 계승한 것
- `payloadLength`
  - TCP는 메시지 경계를 보장하지 않으므로 프레임 길이는 여전히 필요하다.
- `randomKey`
  - 기본 패킷 암호화 입력 값으로 계속 사용한다.
- `checkSum`
  - 수신 시 payload 무결성을 가볍게 확인하는 최소 검증 수단으로 유지했다.

### 3-2. 레거시 구조에서 바꾼 것
- 전역 `PACKET_CODE`
  - 새 구조에서는 전역 설정 의존을 제거했다.
- payload 선두 `WORD type`
  - 응용 계층 규약이 payload 내부 관례에 숨어 있으면 코어와 상위 로직 경계가 흐려진다.
  - 새 구조에서는 `opcode`를 헤더로 승격했다.

### 3-3. Packet Header V1
- 현재 헤더 필드는 아래와 같다.
  - `opcode`
  - `payloadLength`
  - `randomKey`
  - `checkSum`
  - `flags`

## 4. 처리 순서

### 4-1. Send 경로
1. 응용 계층이 `opcode`와 plaintext payload를 전달한다.
2. cipher가 있으면 payload를 암호화한다.
3. 암호화된 payload 기준으로 `checkSum`을 계산한다.
4. 프레이머가 헤더를 붙여 전송 버퍼를 만든다.
5. `WSASend`로 전송한다.

### 4-2. Recv 경로
1. 세션 누적 버퍼에 수신 바이트를 append 한다.
2. 프레이머가 패킷 하나를 추출한다.
3. 추출된 payload 기준으로 `checkSum`을 검증한다.
4. cipher가 있으면 payload를 복호화한다.
5. `opcode`와 plaintext payload를 응용 계층에 전달한다.

## 5. 인터페이스 변경 이유
- [IServer.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\IServer.h)
  - `Send()`에 `opcode`를 추가했다.
- [IApplicationHandler.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\IApplicationHandler.h)
  - `OnPacketReceived()`가 `opcode`를 직접 받도록 바꿨다.

이 변경 덕분에 응용 계층은 더 이상 payload 첫 바이트나 첫 `WORD`를 직접 해석하지 않아도 된다.

## 6. 적용 결과
- [EchoServer/Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)
  - 요청 `opcode`를 보고 응답 `opcode`를 선택한다.
- [EchoClient/Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoClient\Main.cpp)
  - 요청 패킷 생성 시 `opcode`, `checkSum`을 포함한다.
  - 응답 수신 시 `opcode`, `checkSum`을 검증한다.

## 7. 검증 근거
- 빌드 성공:
  - [NetworkLib.vcxproj](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\NetworkLib.vcxproj)
  - [LockFreeTests.vcxproj](D:\Project\ServerPortfolio\RefactoringServer\LockFreeTests\LockFreeTests.vcxproj)
  - [EchoServer.vcxproj](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\EchoServer.vcxproj)
  - [EchoClient.vcxproj](D:\Project\ServerPortfolio\RefactoringServer\EchoClient\EchoClient.vcxproj)
- 테스트 성공:
  - [LockFreeTests.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\LockFreeTests.exe)
    - `Packet framer round trip`
    - `Packet framer partial receive`
  - [EchoServer.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\EchoServer.exe)
  - [EchoClient.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\EchoClient.exe)
    - `response: echo-test`
    - `echo validation succeeded.`

## 8. 현재 한계
- 헤더에 `sequence`, `version`, `magic`은 아직 없다.
- 현재 `flags`는 예약 상태다.
- `EchoClient`는 단일 `recv()`로 응답 하나를 받는 단순 검증용 구조라 다중 패킷 누적 수신 검증은 아직 약하다.

## 9. 결론
- 새 `NetworkLib`는 레거시 패킷 구조를 그대로 복제하지 않았다.
- 대신 레거시에서 유효했던 길이, 난수 키, 체크섬 개념은 계승했다.
- 그리고 payload 선두 타입 관례를 없애고 `opcode`를 헤더로 승격해 응용 계층과 전송 계층 경계를 더 명확히 만들었다.
