# NetworkLib Packet Framing And Cipher Plan

## 1. 목적
- 현재 `EchoServer`/`EchoClient`에서 응용 계층 수준으로만 검증한 패킷 암호화를 `NetworkLib` 코어로 옮기기 위한 선행 설계를 정리한다.
- 목표는 `recv/send` 함수에 바로 암복호화를 섞는 것이 아니라, `패킷 프레이밍 -> 암복호화 -> application dispatch` 경계를 먼저 고정하는 것이다.

## 2. 현재 확인된 사실
- 현재 `EchoServer`와 `EchoClient`는 payload 앞 1바이트를 `randomKey`로 사용하고, 나머지 payload를 `FDefaultPacketCipher`로 처리한다.
- 이 방식으로 실제 왕복 검증은 성공했다.
- 반면 `NetworkLib` 내부의 [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FIocpServer.cpp)는 여전히 “수신된 바이트 덩어리 전체를 그대로 application에 넘기는 구조”다.
- 즉 지금 상태에서 암복호화를 코어에 바로 넣으면 다음 문제를 피하기 어렵다.
  - 패킷 분할 수신
  - 여러 패킷의 연속 수신
  - 프레임 경계 없는 바이트 스트림 처리

## 3. 왜 프레이밍이 먼저 필요한가
- TCP는 메시지 경계를 보장하지 않는다.
- 따라서 `recv()` 한 번이 “패킷 1개”를 의미하지 않는다.
- 코어에 cipher를 올리려면 최소한 아래 순서가 성립해야 한다.
  1. 수신 버퍼 누적
  2. 프레임 헤더 해석
  3. 패킷 단위 추출
  4. payload 복호화
  5. application dispatch

## 4. 1차 프레이밍 기준 제안

### 4-1. 고정 헤더
- 최소 헤더는 아래 2개를 포함한다.
  - `payloadLength`
  - `randomKey`

### 4-2. 권장 헤더 초안
- `std::uint16_t payloadLength`
- `std::uint8_t randomKey`

### 4-3. 해석 순서
- 헤더 크기만큼 수신되었는지 확인
- `payloadLength` 기준으로 본문 길이 확인
- 패킷 전체가 도착했을 때만 payload를 꺼내서 cipher 적용

## 5. 적용 계층

### 5-1. `NetworkLib` 내부에 추가할 것
- `Packet` 또는 `Framing` 디렉터리
- 예시 후보:
  - `SPacketHeader`
  - `IPacketFramer`
  - `FDefaultPacketFramer`

### 5-2. `SServerConfig`에 추가할 후보
- `std::shared_ptr<Crypto::IPacketCipher> packetCipher`
- `bool usePacketFraming`
- 이후 필요하면 `std::shared_ptr<Packet::IPacketFramer> packetFramer`

## 6. 1차 구현 범위
- `recvBuffer`를 바로 application에 넘기지 않고 세션별 누적 버퍼를 둔다.
- `payloadLength + randomKey` 기반 기본 프레이머를 추가한다.
- 프레이밍 완료 후 payload만 `IPacketCipher`에 넘긴다.
- `Send()`도 동일 헤더를 붙인 뒤 payload를 암호화해 송신한다.

## 7. 구현 순서 제안
1. `Packet` 프레이밍 기획 문서 상세화
2. `SPacketHeader`와 기본 프레이머 추가
3. `SServerConfig`에 `packetCipher` 연결
4. `EchoServer`/`EchoClient`를 프레이머 기반으로 바꾸기
5. `FIocpServer` recv/send 경계로 이동

## 8. 검증 계획
- 단일 클라이언트 에코 왕복
- 여러 번 연속 송신
- 긴 메시지 송신
- 분할 수신을 유도하는 작은 recv buffer 환경
- `FDefaultPacketCipher`와 `FNullPacketCipher` 모두 동일 프레이머에서 동작 확인

## 9. 현재 결론
- 다음 작업이 `NetworkLib` 암호화 적용인 것은 맞다.
- 다만 바로 `FIocpServer::Send/Recv`에 cipher를 넣는 것보다, 프레이밍 계층을 먼저 고정하는 것이 안전하다.
- 따라서 다음 실제 코드 작업은 `packet framing` 1차 구현이 가장 자연스럽다.
