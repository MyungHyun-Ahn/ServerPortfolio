# NetworkLib Packet Cipher Plan

## 1. 목적
- 레거시 `MHLib`의 [CEncryption.h](D:\Project\ServerPortfolio\includes\MHLib\security\CEncryption.h)에서 패킷 변환 알고리즘을 가져오되, 현재 `RefactoringServer` 구조에 맞게 다시 설계한다.
- 이 모듈은 `Foundation`이 아니라 `NetworkLib` 내부에 둔다.
- 이유는 이 로직이 범용 보안 모듈보다 `패킷 송수신 경계에서 바이트를 변환하는 네트워크 계층 기능`에 더 가깝기 때문이다.

## 2. 기존 구현에서 가져올 것
- `Encoding`
- `Decoding`
- `CalCheckSum`

## 3. 기존 구현에서 버릴 것
- 전역 `PACKET_KEY`
- `windows.h` 의존
- 정적 전역 상태에 기대는 사용 방식

## 4. 인터페이스 기반 2차 설계 기준
- 경로:
  - `RefactoringServer/NetworkLib/Crypto`
- 공개 타입:
  - `SPacketCipherConfig`
  - `SDefaultPacketCipherConfig`
  - `IPacketCipher`
  - `FDefaultPacketCipher`
  - `FNullPacketCipher`
- 책임:
  - `IPacketCipher`는 packet transform 정책 교체 지점이다.
  - `FDefaultPacketCipher`는 현재 기본 패킷 암호화 구현체다.
  - `FNullPacketCipher`는 패킷을 변형하지 않는 no-op 구현체다.
  - 세션, 소켓, 패킷 헤더 구조에는 직접 기대지 않는다.

## 5. 왜 인터페이스가 필요한가
- 서버별로 암호화 미사용 정책이 필요할 수 있다.
- 테스트용 더미 cipher가 필요할 수 있다.
- 향후 다른 패킷 변환 규칙을 붙일 수 있다.
- 현재는 `FDefaultPacketCipher`와 `FNullPacketCipher`만 있어도, 경계를 먼저 `IPacketCipher`로 잡아두면 상위 계층이 구현체 이름에 덜 묶인다.

## 6. 2차 구현 범위
- `IPacketCipher` 추가
- 기본 알고리즘 구현체를 `FDefaultPacketCipher`로 분리
- `packetKey`를 `SDefaultPacketCipherConfig`에 명시
- `FNullPacketCipher` 추가
- `LockFreeTests`에서 인터페이스 포인터 경유 검증

## 7. 디폴트 패킷 암호화 공식

### 7-1. 입력 값
- `buffer[i]`: 현재 평문 또는 암호문 바이트
- `randomKey`: 패킷별 랜덤 키
- `packetKey`: 서버 설정으로 주입되는 기본 키
- `P`: 이전 plain state
- `E`: 이전 encoded state

### 7-2. 인코딩
- 초기값:
  - `P0 = 0`
  - `E0 = 0`
- 각 바이트 `i`에 대해:
  - `P(i) = buffer[i] XOR (P(i-1) + (randomKey + 1) + i)`
  - `E(i) = P(i) XOR (E(i-1) + (packetKey + 1) + i)`
  - `buffer[i] = E(i)`

### 7-3. 디코딩
- 초기값:
  - `PrevP = 0`
  - `PrevE = 0`
- 각 바이트 `i`에 대해:
  - `P(i) = buffer[i] XOR (PrevE + (packetKey + 1) + i)`
  - `D(i) = P(i) XOR (PrevP + (randomKey + 1) + i)`
  - `PrevE = buffer[i]`
  - `buffer[i] = D(i)`
  - `PrevP = P(i)`

### 7-4. 체크섬
- `checksum = (sum(buffer[i])) mod 256`

### 7-5. 해석 포인트
- 이전 바이트 상태(`P`, `E`)를 다음 바이트 계산에 섞기 때문에 완전히 독립적인 바이트 XOR보다 패턴 반복이 덜 드러난다.
- `randomKey`가 패킷마다 바뀌면 같은 payload라도 결과가 달라질 수 있다.
- 이 알고리즘은 암호학적으로 강한 보안용 cipher라기보다, 게임 서버 패킷 난독화/변환에 가까운 기본 구현으로 보는 것이 맞다.

## 8. 검증 계획
- `NetworkLib` 빌드
- `LockFreeTests` 빌드
- plaintext -> encode -> decode -> plaintext 복구 확인
- checksum이 decode 후 원래 값과 일치하는지 확인
- 테스트 코드가 `IPacketCipher` 포인터로도 동일하게 동작하는지 확인
- null cipher가 payload를 변경하지 않는지 확인

## 9. 다음 단계 후보
- 패킷 프레이밍 계층과 연결
- session send/recv 경계에서 적용할 위치 설계
- 서버 설정에서 cipher 선택 정책 연결
