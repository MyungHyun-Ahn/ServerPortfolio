# Packet Cipher Review

## 1. 문서 목적
- `RefactoringServer/NetworkLib/Crypto` 아래 패킷 암호화 모듈의 현재 구조와 판단 근거를 정리한다.
- 레거시 `CEncryption`에서 무엇을 가져오고 무엇을 버렸는지 기록한다.
- `IPacketCipher`, `FDefaultPacketCipher`, `FNullPacketCipher`의 역할 구분과 현재 검증 범위를 남긴다.

## 2. 현재 구현 범위
- 공용 타입:
  - [PacketCipherTypes.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Crypto\PacketCipherTypes.h)
- 인터페이스:
  - [IPacketCipher.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Crypto\IPacketCipher.h)
- 기본 구현:
  - [FDefaultPacketCipher.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Crypto\FDefaultPacketCipher.h)
  - [FDefaultPacketCipher.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Crypto\FDefaultPacketCipher.cpp)
- no-op 구현:
  - [FNullPacketCipher.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Crypto\FNullPacketCipher.h)
  - [FNullPacketCipher.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Crypto\FNullPacketCipher.cpp)
- 검증 코드:
  - [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\LockFreeTests\Main.cpp)

## 3. 구조 판단 근거

### 3-1. `Foundation`이 아니라 `NetworkLib` 내부에 둔 이유
- 현재 로직은 범용 보안 라이브러리라기보다 패킷 송수신 경계에서 바이트를 변환하는 네트워크 계층 기능에 가깝다.
- 세션/프레이밍/패킷 정책과 함께 움직일 가능성이 커서 `NetworkLib` 내부에 두는 편이 자연스럽다.

### 3-2. 전역 `PACKET_KEY`를 없앤 이유
- 레거시 구현은 전역 키에 기대기 때문에 테스트와 교체가 어렵다.
- 현재는 [SDefaultPacketCipherConfig](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Crypto\PacketCipherTypes.h)로 `packetKey`를 명시적으로 가진다.
- 이 구조 덕분에 상위 계층에서 구현체별 설정을 분리해 넘길 수 있다.

### 3-3. `IPacketCipher`를 먼저 둔 이유
- 상위 계층이 구현체 이름에 묶이지 않게 하기 위한 경계다.
- 현재 필요성은 다음과 같다.
  - 암호화 미사용 정책
  - 테스트용 no-op 정책
  - 향후 다른 패킷 변환 알고리즘 추가
- 지금은 구현체가 많지 않아도, 인터페이스를 먼저 두는 편이 장기적으로 안전하다.

### 3-4. `FDefaultPacketCipher`와 `FNullPacketCipher`를 분리한 이유
- [FDefaultPacketCipher.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Crypto\FDefaultPacketCipher.cpp)는 레거시 알고리즘을 현재 구조로 옮긴 기본 구현이다.
- [FNullPacketCipher.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Crypto\FNullPacketCipher.cpp)는 payload를 건드리지 않는 no-op 구현이다.
- 이 두 구현체만으로도 `IPacketCipher`가 단순 래퍼가 아니라 실제 정책 교체 지점임을 확인할 수 있다.

## 4. 알고리즘 판단
- `FDefaultPacketCipher`는 이전 plain state와 이전 encoded state를 다음 바이트 계산에 섞는다.
- 따라서 단순한 바이트별 독립 XOR보다 패턴 반복이 줄어든다.
- 다만 이 구현은 강한 암호학적 보안 cipher라기보다 게임 서버 패킷 난독화/변환용 기본 구현으로 보는 것이 맞다.

## 5. 확인된 장점
- 전역 상태 없이 설정 기반으로 동작한다.
- 인터페이스와 구현체가 분리돼 있다.
- no-op 구현체가 있어 테스트와 정책 교체가 쉬워졌다.
- 체크섬 계산도 같은 경계 안에서 일관되게 제공한다.

## 6. 현재 적용 상태
- 현재 `Default` 패킷 암호화는 `NetworkLib` 코어 내부 송수신 경계가 아니라 응용 계층 검증용으로 먼저 연결돼 있다.
- 적용 지점:
  - [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)
  - [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoClient\Main.cpp)
- 현재 검증 방식:
  - 요청/응답 payload 앞 1바이트를 `randomKey`로 사용
  - 나머지 payload를 `FDefaultPacketCipher`로 인코딩/디코딩
  - `EchoClient`가 복호화 후 원문 `echo-test`와 일치하는지 확인

## 7. 현재 한계와 리스크

### 7-1. `NetworkLib` 코어 송수신 경계에는 아직 직접 연결되지 않았다
- 현재는 `EchoServer`/`EchoClient` 응용 계층에서 암복호화를 수행한다.
- 실제 `NetworkLib` 내부 send/recv 경계, 패킷 프레이밍 계층, session 경계에는 아직 미적용이다.

### 7-2. 인터페이스 경계는 생겼지만 생성 정책은 아직 없다
- 지금은 테스트 코드에서 직접 구현체를 생성한다.
- 이후 서버 설정과 팩토리 정책을 연결해야 실제 운영 경로에서 의미가 생긴다.

### 7-3. `FNullPacketCipher`도 체크섬은 계산한다
- no-op cipher라도 현재는 체크섬 계산을 제공한다.
- 이 정책이 맞는지는 이후 패킷 프레이밍 계층 설계와 함께 다시 검토할 수 있다.

## 8. 검증 근거
- 빌드:
  - [NetworkLib.vcxproj](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\NetworkLib.vcxproj)
  - [LockFreeTests.vcxproj](D:\Project\ServerPortfolio\RefactoringServer\LockFreeTests\LockFreeTests.vcxproj)
  - [EchoServer.vcxproj](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\EchoServer.vcxproj)
  - [EchoClient.vcxproj](D:\Project\ServerPortfolio\RefactoringServer\EchoClient\EchoClient.vcxproj)
- 실행:
  - [LockFreeTests.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\LockFreeTests.exe)
  - [EchoServer.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\EchoServer.exe)
  - [EchoClient.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\EchoClient.exe)
- 확인된 테스트:
  - `Packet cipher round trip`
  - `Null packet cipher`
  - 암호화된 `echo-test` 요청/응답 왕복
- 두 테스트 모두 `IPacketCipher` 포인터 경유로 호출했고 PASS를 확인했다.
 - `EchoClient`는 복호화 후 `response: echo-test`, `echo validation succeeded.`를 출력했다.

## 9. 다음 작업 후보
- 서버 설정에서 cipher 선택 정책 연결
- 패킷 프레이밍 계층과 연동
- `EchoServer` 송수신 경로에 실제 적용
