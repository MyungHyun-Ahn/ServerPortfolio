# 수신 패킷 역직렬화 zero-copy 확대 계획

## 1. 목적
- recv 경로에서 복사 없이 읽을 수 있는 데이터는 최대한 view 형태로 다룬다.
- `PacketGenerator`가 view 타입을 생성할 수 있게 만들어 콘텐츠 스키마에서 선택적으로 zero-copy 역직렬화를 사용할 수 있게 한다.

## 2. 현재 문제
- `string` 필드는 recv payload에서 읽을 때 항상 새 버퍼로 복사된다.
- `FPacketView`까지는 zero-copy인데, generated packet `Deserialize()` 단계에서 다시 소유형 타입으로 바뀌면서 복사가 발생한다.

## 3. 적용 방향
### 3-1. 스키마 타입 추가
- `string_view`
- 이후 필요하면 `bytes_view`

### 3-2. 런타임 지원
- `FPacketReader`에 `std::string_view` 읽기 경로를 추가한다.
- `FPacketWriter`는 `std::string_view`를 그대로 쓸 수 있게 한다.
- `GetSerializedSize()`에도 `std::string_view` 오버로드를 추가한다.

### 3-3. 생성 코드
- `PacketGenerator`가 `string_view`를 C++에서는 `std::string_view`로 생성한다.
- C# 출력 타입은 우선 `string`으로 매핑한다.
- generated packet은 기존처럼 `Serialize/Deserialize`를 가지되, view 타입 필드는 recv callback 범위 안에서만 유효하다는 전제를 문서화한다.

### 3-4. 샘플 적용
- `Echo` 패킷의 `message`를 `string_view`로 전환한다.
- `EchoServer`, `EchoClient`가 generated Echo packet을 그대로 사용하면서 회귀를 통과하는지 확인한다.

## 4. 제한
- view 타입은 payload 버퍼 수명에 의존한다.
- 따라서 callback 범위를 넘어 오래 저장하려면 호출 측에서 명시적으로 복사해야 한다.

## 5. 검증 계획
- generated Echo packet round-trip 재검증
- Echo 서버/클라이언트 로그인 후 에코 왕복 검증
- 기존 컨테이너/packet framer 회귀 테스트 통과 확인
