# 패킷 생성 코드 최적화 계획

## 1. 목적
- `PacketGenerator`가 생성하는 C++ 패킷 코드가 런타임 최적화 포인트를 활용할 수 있게 만든다.
- 생성 코드가 `FPacketWriter`, `FPacketReader`의 개선을 바로 타도록 구조를 정리한다.

## 2. 현재 문제
- 생성된 패킷 클래스는 `Serialize`, `Deserialize`만 단순 호출한다.
- 직렬화 전에 예상 크기를 알려주지 못해 writer가 여러 번 확장될 수 있다.
- 생성 코드 차원에서 성능 관련 의도가 드러나지 않는다.

## 3. 적용 방향
### 3-1. 공통 인터페이스 확장
- `IContentPacket`에 `GetEstimatedBodySize()`를 추가한다.
- 기본 구현은 `0`을 반환하고, generated packet은 추정 크기를 계산해서 override 한다.

### 3-2. Generated packet
- scalar 필드와 고정 길이 array는 정적 크기를 합산한다.
- `string`, `vector`, `map`, `unordered_map`는 길이 prefix와 원소 크기를 기준으로 동적 추정값을 계산한다.
- `Serialize()` 시작 시 `writer.ReserveAdditional(GetEstimatedBodySize())`를 호출한다.

### 3-3. 범위
- nested container는 여전히 공식 지원 범위 밖으로 둔다.
- 커스텀 직렬화 override 경로는 그대로 유지한다.

## 4. 기대 효과
- writer 확장 횟수 감소
- generated packet이 커질수록 힙/버퍼 재조정 비용 감소

## 5. 검증 계획
- `PacketGenerator` 재생성
- Echo/Login/Chat generated packet 빌드 확인
- 기존 round-trip 테스트 통과 확인
