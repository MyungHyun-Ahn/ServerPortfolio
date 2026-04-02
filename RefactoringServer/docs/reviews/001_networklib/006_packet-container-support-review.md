# Packet Container Support Review

## 1. 목적
- `PacketGenerator`가 C++ 컨테이너 타입을 실제 스키마에서 생성하고, 런타임 `Serialize/Deserialize`까지 정상 동작하는지 확인한다.

## 2. 검증 대상
- 스키마:
  - [Chat.yaml](D:\Project\ServerPortfolio\RefactoringServer\Packet\Chat\Chat.yaml)
- generated packet:
  - [ChatPackets.h](D:\Project\ServerPortfolio\RefactoringServer\Generated\Packets\Chat\ChatPackets.h)
- generated handler:
  - [ChatPacketHandler.h](D:\Project\ServerPortfolio\RefactoringServer\Generated\Packets\Chat\ChatPacketHandler.h)
- 공통 라우터:
  - [PacketRouter.h](D:\Project\ServerPortfolio\RefactoringServer\Generated\Packets\PacketRouter.h)
- 런타임 테스트:
  - [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\LockFreeTests\Main.cpp)

## 3. 이번에 확인한 타입
- `std::vector<std::string>`
- `std::map<std::string, std::uint32_t>`
- `std::unordered_map<std::string, std::string>`

## 4. 구조 판단
- 컨테이너 지원은 생성기와 런타임 helper가 같은 타입 체계를 공유해야 안정적이다.
- 지금 구조는 YAML 스키마 타입을 `PacketGenerator`가 C++ 타입으로 변환하고, generated packet이 `writer.Write(...)`, `reader.Read(...)` 멤버를 직접 호출하는 방식이라 읽기 쉽고 추적이 쉽다.
- `Chat`처럼 두 번째 콘텐츠를 추가했을 때도 generated router가 자연스럽게 확장되어, 컨테이너 지원이 Echo 전용 샘플에 묶이지 않음을 확인했다.

## 5. 검증 결과
- `Generate-Packets.cmd` 실행 성공
- `ChatPackets.h` 생성 성공
- `LockFreeTests.exe`에서 아래 항목 통과:
  - `Generated chat container packet round trip`

## 6. 현재 한계
- nested container는 아직 실사용 검증을 하지 않았다.
- `unordered_map`은 순서가 보장되지 않으므로 직렬화 결과 바이트 비교보다 역직렬화 후 값 비교 중심으로 검증해야 한다.
- C# 출력은 타입 매핑 규칙만 갖고 있고, 실제 C# 패킷 산출물은 아직 만들지 않았다.

## 7. 결론
- 현재 구조는 `vector`, `map`, `unordered_map` 수준의 1차 컨테이너 지원을 시작하기에 충분하다.
- 다음 확장 우선순위는 `nested container 제한 정책`과 `C# 출력물 생성`이다.
