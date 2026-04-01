# Packet Container Support Policy

## 1. 현재 정책
- `PacketGenerator`의 공식 지원 범위는 컨테이너 1단계까지다.
- 즉 패킷 필드는 scalar, `string`, 그리고 컨테이너 1개까지만 자동 생성 대상으로 본다.

## 2. 허용 범위
- 허용:
  - `vector<string>`
  - `array<uint32, 8>`
  - `map<string, uint32>`
  - `unordered_map<string, string>`
- 조건:
  - `vector<T>`의 `T`는 다시 컨테이너가 아니어야 한다.
  - `map<K, V>`의 `K`, `V`는 다시 컨테이너가 아니어야 한다.
  - `unordered_map<K, V>`도 같은 규칙을 따른다.

## 3. 금지 범위
- 금지:
  - `vector<vector<int32>>`
  - `map<string, vector<uint32>>`
  - `unordered_map<string, map<string, int32>>`
- 이런 nested container는 생성기 기본 지원에서 제외한다.

## 4. 이유
- 타입 파싱 복잡도가 급격히 커진다.
- C++ / C# 양쪽 타입 매핑 규칙이 불필요하게 어려워진다.
- 디버깅과 직렬화 포맷 추적이 더 힘들어진다.
- 대부분의 실무 패킷은 1단계 컨테이너로도 충분하다.

## 5. 예외 처리 방식
- 복잡한 구조가 정말 필요하면 generated 기본 구현을 그대로 쓰지 않고, 패킷 클래스에서 `Serialize` / `Deserialize`를 직접 override해서 해결한다.
- 즉 기본 경로는 자동 생성, 예외 경로는 수동 직렬화라는 원칙을 유지한다.

## 6. 현재 검증 근거
- [Chat.yaml](D:\Project\ServerPortfolio\RefactoringServer\Packet\Chat\Chat.yaml)
- [ChatPackets.h](D:\Project\ServerPortfolio\RefactoringServer\Generated\Packets\Chat\ChatPackets.h)
- [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\LockFreeTests\Main.cpp)
- [packet-container-support-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\networklib\packet-container-support-review.md)
