# 수신 역직렬화 zero-copy 리뷰

## 1. 목적
- recv 경로에서 복사 없이 읽을 수 있는 데이터는 view 형태로 다루는 방향이 실제 구조에 맞는지 정리한다.

## 2. 이번 단계에서 적용한 내용
### 2-1. 런타임 지원
- [FPacketReader.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FPacketReader.h)
  - `std::string_view` 읽기 경로를 추가했다.
- [FPacketWriter.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FPacketWriter.h)
  - `std::string_view` 쓰기 경로를 추가했다.
- [FPacketSerialization.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FPacketSerialization.h)
  - `std::string_view` 직렬화 크기 계산 경로를 추가했다.

### 2-2. 생성기 지원
- [Program.cs](D:\Project\ServerPortfolio\RefactoringServer\Tools\PacketGenerator\Program.cs)
  - 스키마 타입 `string_view`를 C++에서 `std::string_view`로 생성하도록 추가했다.
  - C# 출력 타입은 우선 `string`으로 유지했다.

### 2-3. 샘플 적용
- [Echo.yaml](D:\Project\ServerPortfolio\RefactoringServer\Packet\Echo\Echo.yaml)
  - `message` 필드를 `string_view`로 전환했다.
- [EchoPackets.h](D:\Project\ServerPortfolio\RefactoringServer\Generated\Packets\Echo\EchoPackets.h)
  - generated Echo packet이 `std::string_view`를 사용하도록 재생성됐다.
- [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)
- [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoClient\Main.cpp)
  - 샘플 서버/클라이언트가 이 경로를 사용하면서도 회귀가 깨지지 않도록 조정했다.

## 3. 구조적 장점
- `FPacketView` 이후 generated packet 역직렬화 단계까지 zero-copy 범위를 더 넓힐 수 있다.
- 특히 읽기 전용 문자열 payload는 recv 버퍼를 그대로 참조하므로 불필요한 복사를 줄일 수 있다.
- 스키마 차원에서 view 타입을 선택할 수 있으므로, 패킷별로 소유형과 비소유형을 명시적으로 고를 수 있다.

## 4. 가장 중요한 제한
- `std::string_view`는 recv payload 버퍼 수명에 의존한다.
- 따라서 handler callback 범위를 넘겨 오래 보관하려면 반드시 호출 측에서 `std::string`으로 복사해야 한다.
- 즉 이 최적화는 `즉시 처리형 패킷`에 특히 잘 맞는다.

## 5. 현재 판단
- Echo 같은 짧은 메시지 왕복에서도 구조적으로 잘 맞는다.
- 다만 모든 패킷을 일괄적으로 view 타입으로 바꾸는 건 맞지 않다.
- 오래 보관하는 데이터, 컨테이너에 누적하는 데이터는 여전히 소유형 타입이 더 안전하다.

## 6. 검증 근거
- 빌드
  - `RefactoringServer.sln` x64 Debug
- 테스트
  - [LockFreeTests.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\LockFreeTests.exe)
- 런타임 스모크
  - [recv_view_smoke_server.log](D:\Project\ServerPortfolio\RefactoringServer\Out\recv_view_smoke_server.log)
  - [recv_view_smoke_client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\recv_view_smoke_client.log)

## 7. 결론
- recv 역직렬화 zero-copy 확대는 현재 구조와 잘 맞는다.
- 다만 view 타입은 `성능 최적화용 선택지`로 보고, 수명 규칙이 안전한 패킷에만 선택적으로 적용하는 것이 맞다.
