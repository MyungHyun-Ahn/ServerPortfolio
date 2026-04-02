# bytes_view zero-copy 리뷰

## 1. 목적
- recv 역직렬화 경로에서 바이너리 payload를 복사 없이 읽을 수 있도록 `bytes_view` 지원을 추가한 이유와 현재 제약을 정리한다.

## 2. 이번 단계에서 적용한 내용
### 2-1. 런타임 지원
- [FPacketReader.h](D:/Project/ServerPortfolio/RefactoringServer/NetworkLib/Packet/FPacketReader.h)
  - `std::span<const std::uint8_t>` 읽기 경로를 추가했다.
- [FPacketWriter.h](D:/Project/ServerPortfolio/RefactoringServer/NetworkLib/Packet/FPacketWriter.h)
  - `std::span<const std::uint8_t>` 쓰기 경로를 추가했다.
- [FPacketSerialization.h](D:/Project/ServerPortfolio/RefactoringServer/NetworkLib/Packet/FPacketSerialization.h)
  - `bytes_view` 직렬화 크기 계산 경로를 추가했다.

### 2-2. 생성기 지원
- [Program.cs](D:/Project/ServerPortfolio/RefactoringServer/Tools/PacketGenerator/Program.cs)
  - 스키마 타입 `bytes_view`를 C++에서는 `std::span<const std::uint8_t>`로 생성하도록 추가했다.
  - C# 출력 타입은 우선 `byte[]`로 매핑한다.

### 2-3. 샘플 적용
- [Chat.yaml](D:/Project/ServerPortfolio/RefactoringServer/Packet/Chat/Chat.yaml)
  - `RoomBinarySnapshotNoti.payload`를 `bytes_view`로 정의했다.
- [ChatPackets.h](D:/Project/ServerPortfolio/RefactoringServer/Generated/Packets/Chat/ChatPackets.h)
  - generated packet이 `std::span<const std::uint8_t>`를 사용하도록 생성됐다.

## 3. 구조적 장점
- 큰 바이너리 payload를 `std::vector<std::uint8_t>`로 한 번 더 복사하지 않고 recv 버퍼를 그대로 참조할 수 있다.
- `string_view`와 같은 규칙으로 다뤄서 생성기와 런타임 설계가 일관된다.
- 이후 압축 데이터, blob, 직렬화된 하위 구조 payload에 대해 zero-copy 선택지를 열어둘 수 있다.

## 4. 가장 중요한 수명 규칙
- `bytes_view`는 recv payload 버퍼 수명에 의존한다.
- handler callback 범위를 넘겨 오래 보관하면 안 된다.
- 오래 들고 있어야 하면 호출 측에서 `std::vector<std::uint8_t>`나 자체 버퍼로 명시적으로 복사해야 한다.

## 5. 현재 제약
- 현재 zero-copy 대상은 읽기 전용 바이너리 view다.
- 수정 가능한 view는 아직 지원하지 않는다.
- 컨테이너 중첩 정책과 마찬가지로, 복잡한 수명 관리가 필요한 경우는 기본 생성 경로보다 수동 구현이 더 안전하다.

## 6. 검증 근거
- 빌드
  - `RefactoringServer.sln` x64 Debug
- 테스트
  - [LockFreeTests.exe](D:/Project/ServerPortfolio/RefactoringServer/Out/LockFreeTests.exe)
  - `Packet bytes_view round trip`
  - `Generated chat bytes_view packet round trip`
- 스모크
  - [bytes_view_smoke_server.log](D:/Project/ServerPortfolio/RefactoringServer/Out/bytes_view_smoke_server.log)
  - [bytes_view_smoke_client.log](D:/Project/ServerPortfolio/RefactoringServer/Out/bytes_view_smoke_client.log)

## 7. 결론
- `bytes_view`는 recv zero-copy 확장의 다음 단계로 구조적으로 타당하다.
- 다만 성능 최적화용 선택지로 보고, 수명 규칙을 지킬 수 있는 패킷에만 선택적으로 적용하는 것이 맞다.
