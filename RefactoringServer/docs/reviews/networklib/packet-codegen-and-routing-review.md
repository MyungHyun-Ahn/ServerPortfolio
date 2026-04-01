# Packet CodeGen And Routing Review

## 1. 문서 목적
- 현재 `PacketGenerator`, `ContentHeader`, generated packet/handler, 상위 packet router 구조를 한 문서에서 파악할 수 있게 정리한다.
- 패킷 생성 규칙과 콘텐츠별 처리기 분리 방향이 왜 필요한지 근거를 남긴다.

## 2. 현재 구조

### 2-1. 스키마 루트
- 패킷 스키마 최상위 루트는 [Packet](D:\Project\ServerPortfolio\RefactoringServer\Packet)이다.
- 콘텐츠별 하위 디렉터리로 나눈다.
- 현재 예시:
  - [Echo.yaml](D:\Project\ServerPortfolio\RefactoringServer\Packet\Echo\Echo.yaml)

### 2-2. 생성기
- C# 오프라인 생성기:
  - [PacketGenerator.csproj](D:\Project\ServerPortfolio\RefactoringServer\Tools\PacketGenerator\PacketGenerator.csproj)
  - [Program.cs](D:\Project\ServerPortfolio\RefactoringServer\Tools\PacketGenerator\Program.cs)
- 수동 실행 진입점:
  - [Generate-Packets.ps1](D:\Project\ServerPortfolio\RefactoringServer\scripts\Generate-Packets.ps1)
  - [Generate-Packets.cmd](D:\Project\ServerPortfolio\RefactoringServer\scripts\Generate-Packets.cmd)

### 2-3. 생성 결과
- generated packet:
  - [EchoPackets.h](D:\Project\ServerPortfolio\RefactoringServer\Generated\Packets\Echo\EchoPackets.h)
- 콘텐츠별 generated handler:
  - [EchoPacketHandler.h](D:\Project\ServerPortfolio\RefactoringServer\Generated\Packets\Echo\EchoPacketHandler.h)
- 상위 generated router:
  - [PacketRouter.h](D:\Project\ServerPortfolio\RefactoringServer\Generated\Packets\PacketRouter.h)

### 2-4. 런타임 공통 기반
- 공통 패킷 인터페이스:
  - [IContentPacket.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\IContentPacket.h)
- content header:
  - [ContentHeader.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\ContentHeader.h)
- writer / reader:
  - [FPacketWriter.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FPacketWriter.h)
  - [FPacketReader.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FPacketReader.h)
- content packet helper:
  - [FPacketSerialization.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FPacketSerialization.h)

## 3. 핵심 설계 판단

### 3-1. `opcode`는 transport header가 아니라 content header에 둔다
- transport header는 길이, 암호화 키, 체크섬처럼 전송 계층 정보만 가진다.
- 실제 메시지 의미인 `opcode`는 payload 선두의 [ContentHeader.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\ContentHeader.h)로 내렸다.
- 이 방향이 `NetworkLib`와 콘텐츠 계층 경계를 더 명확하게 만든다.

### 3-2. generated packet은 공통 인터페이스를 따른다
- generated packet은 모두 [IContentPacket.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\IContentPacket.h)를 따른다.
- 공통 계약:
  - `GetOpcode()`
  - `Serialize(FPacketWriter&)`
  - `Deserialize(FPacketReader&)`
- payload 멤버는 각 패킷 클래스에만 둔다.
  - 예: [EchoPackets.h](D:\Project\ServerPortfolio\RefactoringServer\Generated\Packets\Echo\EchoPackets.h)의 `message`

### 3-3. `WriteValue` / `ReadValue` free function 대신 writer/reader 멤버를 쓴다
- 생성 코드가 `writer.Write(...)`, `reader.Read(...)`를 직접 호출한다.
- 관련 구현:
  - [FPacketWriter.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FPacketWriter.h)
  - [FPacketReader.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FPacketReader.h)
- 이 방향이 생성 코드 가독성과 IDE 탐색성 면에서 더 낫다.

### 3-4. `Rq` / `Rp`는 반드시 쌍으로 존재해야 한다
- 생성기는 `Rq`만 있거나 `Rp`만 있으면 실패한다.
- `Noti`는 선택 사항이다.
- 이 규칙은 생성기 검증 로직에 들어 있다:
  - [Program.cs](D:\Project\ServerPortfolio\RefactoringServer\Tools\PacketGenerator\Program.cs)

### 3-5. 같은 카테고리는 처리기 1개, 상위 선택은 router가 맡는다
- `Chat` 같은 콘텐츠 카테고리 아래 메시지가 여러 개 있어도 처리기 base는 1개다.
  - 예: `FChatPacketHandlerBase`
- 대신 상위 [PacketRouter.h](D:\Project\ServerPortfolio\RefactoringServer\Generated\Packets\PacketRouter.h)가 `opcode -> content handler` 선택을 맡는다.
- 이 구조가 “모든 패킷을 전역 처리기 1개에 몰아넣는 방식”보다 확장성이 좋다.

## 4. 현재 Echo 적용 방식
- Echo 서버는 generated router를 통해 packet을 처리한다:
  - [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)
- 흐름:
  1. `FIocpServer`가 transport payload를 복호화/검증한다.
  2. [FPacketSerialization.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FPacketSerialization.h)에서 `ContentHeader`를 파싱한다.
  3. `packetView.opcode`를 얻는다.
  4. [PacketRouter.h](D:\Project\ServerPortfolio\RefactoringServer\Generated\Packets\PacketRouter.h)가 `Echo` handler로 넘긴다.
  5. [EchoPacketHandler.h](D:\Project\ServerPortfolio\RefactoringServer\Generated\Packets\Echo\EchoPacketHandler.h)가 구체 패킷 객체를 만들고 `Deserialize` 후 `HandleEchoRq` 등을 호출한다.

## 5. 수동 생성 정책
- 현재 정책은 “스키마가 바뀔 때만 수동 생성”이다.
- 이유:
  - C# 생성기를 항상 빌드에 물리면 C++ 반복 빌드가 느려진다.
  - 평소 C++ 수정만 할 때는 생성기가 필요 없다.
- 표준 실행:
  - [Generate-Packets.cmd](D:\Project\ServerPortfolio\RefactoringServer\scripts\Generate-Packets.cmd)

## 6. 검증 근거
- 생성기 실행 성공:
  - [Generate-Packets.cmd](D:\Project\ServerPortfolio\RefactoringServer\scripts\Generate-Packets.cmd)
- generated packet round-trip 테스트 통과:
  - [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\LockFreeTests\Main.cpp)
  - [LockFreeTests.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\LockFreeTests.exe)
- Echo 실제 왕복 성공:
  - [EchoServer.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\EchoServer.exe)
  - [EchoClient.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\EchoClient.exe)

## 7. 현재 장점
- 스키마, generated 코드, 런타임 helper의 경계가 분명하다.
- 콘텐츠별 처리기와 상위 라우터가 분리돼 확장에 유리하다.
- 향후 `Chat`, `Login`, `Inventory` 같은 콘텐츠를 같은 패턴으로 추가하기 쉽다.
- schema type -> C++ / C# type 매핑 기반이라 나중에 C# 클라이언트 생성기 확장 여지가 있다.

## 8. 남은 과제
- `Chat`이나 `Login` 같은 두 번째 콘텐츠를 추가해서 multi-content router를 실제로 검증해야 한다.
- generated output 관리 규칙을 더 분명히 할 필요가 있다.
  - git 커밋 포함 여부
  - 재생성 시점 규칙
- 장기적으로는 generated handler 사용 예시를 `EchoServer/Main.cpp` 밖의 실제 `Contents/<Category>` 클래스로 분리하는 것이 좋다.

## 9. 결론
- 현재 구조는 “단일 Echo 샘플”을 넘어서 “콘텐츠별 패킷 생성 + 콘텐츠별 처리기 + 상위 라우터”까지 이어지는 1차 골격으로 충분히 의미가 있다.
- 특히 `opcode`를 content header로 분리하고, generated router가 콘텐츠 선택을 맡도록 한 점이 이후 MMORPG 콘텐츠 확장에 유리한 기반이다.
