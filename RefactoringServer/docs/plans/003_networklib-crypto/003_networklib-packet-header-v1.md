# NetworkLib Packet Header V1

## 1. 목적
- 레거시 프로젝트의 패킷 헤더 의도(`len`, `randKey`, `checkSum`)는 유지하되, 새 `NetworkLib` 기준으로 헤더 책임과 payload 책임을 다시 분리한다.
- payload 선두 `WORD type` 관례를 걷어내고, 패킷 종류를 헤더 `opcode`로 승격한다.
- 전역 `PACKET_CODE`, `PACKET_KEY` 같은 설정 의존을 제거하고, 패킷 경계는 프레이머와 cipher 주입으로 제어한다.

## 2. 레거시 구조에서 가져갈 것
- `payloadLength`
  - TCP 누적 버퍼에서 프레임 경계를 식별하는 데 필요하다.
- `randomKey`
  - 기본 패킷 암호화의 입력 값으로 유지한다.
- `checkSum`
  - 수신 시 payload 변조 여부를 가볍게 확인하는 최소 검증 수단으로 유지한다.

## 3. 레거시 구조에서 바꿀 것
- `PACKET_CODE`
  - 전역 설정과 강결합된 값이므로 제거한다.
- payload 선두 `WORD type`
  - 패킷 해석 규칙이 payload 내부 관례에 숨어 있으면 응용 계층과 전송 계층 경계가 흐려진다.
  - 새 구조에서는 `opcode`를 헤더에 올린다.

## 4. Packet Header V1 제안

### 4-1. 필드 구성
- `std::uint16_t opcode`
- `std::uint16_t payloadLength`
- `std::uint8_t randomKey`
- `std::uint8_t checkSum`
- `std::uint8_t flags`

### 4-2. 필드 의미
- `opcode`
  - 응용 계층에서 해석할 패킷 종류
- `payloadLength`
  - payload 바이트 길이
- `randomKey`
  - `IPacketCipher` 입력용 임시 키
- `checkSum`
  - payload 기준 단순 합산 체크섬
- `flags`
  - 압축, 분할, 예약 비트 등 이후 확장용

## 5. 처리 순서

### 5-1. Send
1. 응용 계층이 `opcode`, plaintext payload를 전달한다.
2. cipher가 있으면 payload를 암호화한다.
3. 암호화된 payload 기준으로 `checkSum`을 계산한다.
4. 헤더를 붙여 framed packet을 만든다.
5. `WSASend`로 전송한다.

### 5-2. Recv
1. 세션 누적 버퍼에 수신 바이트를 append 한다.
2. 프레이머가 헤더를 읽어 패킷 하나를 추출한다.
3. 추출된 payload 기준으로 `checkSum`을 검증한다.
4. cipher가 있으면 payload를 복호화한다.
5. `opcode`와 plaintext payload를 응용 계층에 전달한다.

## 6. 인터페이스 변경 방향
- `IServer::Send()`
  - `sessionId`, `opcode`, `payload`, `payloadLength`
- `IApplicationHandler::OnPacketReceived()`
  - `sessionId`, `opcode`, `payload`, `payloadLength`
- `IPacketFramer`
  - `SOutgoingPacket`, `SFramedPacket` 기준으로 헤더 필드 전체를 다룬다.

## 7. 1차 적용 범위
- `FDefaultPacketFramer`를 Packet Header V1 형식으로 수정
- `FIocpServer` recv/send 경로에서 `opcode`와 `checkSum` 사용
- `EchoServer`, `EchoClient`를 `opcode` 기반 에코 검증으로 전환
- `LockFreeTests`에 헤더 round trip 검증 보강

## 8. 검증 기준
- `Packet framer round trip`
  - `opcode`, `randomKey`, `checkSum`, payload가 모두 보존되는지 확인
- `Packet framer partial receive`
  - 분할 수신 중에도 동일 헤더가 유지되는지 확인
- `EchoServer` / `EchoClient`
  - 요청 `opcode`와 응답 `opcode`가 의도대로 왕복되는지 확인

## 9. 결론
- 새 `NetworkLib`는 레거시 구조를 그대로 복제하지 않는다.
- 대신 레거시 구조에서 유효했던 헤더 의도를 계승하고, `opcode`를 헤더로 승격한 Packet Header V1으로 정리한다.
