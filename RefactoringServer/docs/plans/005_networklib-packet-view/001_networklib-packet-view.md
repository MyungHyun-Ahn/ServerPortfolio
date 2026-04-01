# NetworkLib Packet View Plan

## 1. 목표
- 레거시 프로젝트의 `recv buffer + view wrapper` 의도를 현재 `RefactoringServer` 구조에 맞게 가져온다.
- recv 경로에서 payload를 별도 `std::vector<char>`로 복사하지 않고, 가능하면 세션 recv buffer 위의 view로 콘텐츠 계층에 넘긴다.
- 다만 view 수명주기는 명확히 제한해서, 콜백 범위를 벗어난 장기 보관은 허용하지 않는다.

## 2. 현재 문제
- 현재 [`FDefaultPacketFramer`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FDefaultPacketFramer.cpp)는 recv ring buffer에서 패킷을 찾은 뒤 payload를 `std::vector<char>`로 복사한다.
- 이 구조는 구현이 단순하지만, 레거시의 "view만 넘겨 복사 비용을 줄인다"는 장점이 아직 살아 있지 않다.

## 3. 설계 방향
### 3-1. Packet view 타입 도입
- `Packet` 디렉터리에 `SFramedPacketView` 또는 `FPacketView` 타입을 둔다.
- 포함 필드
  - `opcode`
  - `randomKey`
  - `checkSum`
  - `const char* payload`
  - `std::int32_t payloadLength`

### 3-2. 유효 범위
- packet view는 `OnPacketReceived` 콜백이 끝날 때까지만 유효하다고 본다.
- 콜백 밖으로 저장하려면 상위 계층이 직접 복사한다.
- 이 규칙을 문서와 인터페이스 코멘트로 명확히 남긴다.

### 3-3. recv buffer 선형화
- ring buffer는 payload가 wrap-around 될 수 있다.
- `PacketFramer`가 view를 만들기 전에 필요한 길이만큼 recv buffer를 선형화할 수 있어야 한다.
- 이 선형화는 "가능하면 복사 없음, wrap된 경우에만 내부 정렬" 정책으로 본다.

## 4. 적용 범위
- [`FRecvBuffer`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FRecvBuffer.h)
- [`IPacketFramer`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\IPacketFramer.h)
- [`FDefaultPacketFramer`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FDefaultPacketFramer.cpp)
- [`IApplicationHandler`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\IApplicationHandler.h)
- [`FIocpServer`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FIocpServer.cpp)
- [`EchoServer`](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)

## 5. 검증 계획
- `LockFreeTests`에 recv buffer 기반 packet view 테스트 추가
- `EchoServer --headless` + `EchoClient` 분할 송신/작은 recv 버퍼 재검증
- 리뷰 문서에 현재 구조와 한계를 정리

## 6. 남길 한계
- payload가 wrap된 경우 내부 선형화 복사는 여전히 발생할 수 있다.
- 완전한 zero-copy는 "항상 contiguous 하게 payload가 도착한다"는 가정이 없으면 어렵다.
- 이번 1차 목표는 "불필요한 별도 payload 복사 제거"이지, 모든 경우 무복사를 보장하는 것은 아니다.
