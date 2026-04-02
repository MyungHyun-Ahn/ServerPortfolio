# 패킷 직렬화 경로 최적화 계획

## 1. 목적
- `FPacketReader`, `FPacketWriter`의 공통 경로에서 불필요한 재할당과 원소 단위 반복 복사를 줄인다.
- `vector`, `array` 같은 빈번한 컨테이너의 scalar 경로를 bulk 처리로 바꾼다.

## 2. 현재 문제
- `std::vector<T>`는 `T`가 scalar여도 원소 단위 `Write`, `Read`를 반복한다.
- `std::array<T, N>`도 같은 방식으로 원소마다 호출이 들어간다.
- 컨테이너 payload가 커질수록 함수 호출 수와 분기 수가 늘어난다.

## 3. 적용 방향
### 3-1. Writer
- `vector<T>`에서 `T`가 scalar면 `WriteBytes()` 한 번으로 기록한다.
- `array<T, N>`에서 `T`가 scalar면 `WriteBytes()` 한 번으로 기록한다.

### 3-2. Reader
- `vector<T>`에서 `T`가 scalar면 `resize()` 후 `ReadBytes()` 한 번으로 읽는다.
- `array<T, N>`에서 `T`가 scalar면 `ReadBytes()` 한 번으로 읽는다.

### 3-3. 안전성
- scalar bulk 경로는 `CPacketReadableScalar`, `CPacketWritableScalar` 제약 안에서만 사용한다.
- `string`, `map`, `unordered_map`, nested container는 기존 안전한 경로를 유지한다.

## 4. 기대 효과
- 큰 `vector<int32>`, `vector<uint8>`, `array<float, N>` 같은 payload에서 직렬화 비용 감소
- generated packet이 많아져도 공통 런타임 계층에서 자동 이득 확보

## 5. 검증 계획
- `LockFreeTests`에 scalar vector/array round-trip 테스트 추가
- `Chat` 컨테이너 패킷 round-trip 회귀 확인
- Echo 3분 벤치마크에서 CPU와 TPS를 같이 비교
