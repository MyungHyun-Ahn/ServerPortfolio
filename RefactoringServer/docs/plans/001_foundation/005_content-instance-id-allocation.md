# Content Instance ID 할당 정책

## 1. 목적
- `ContentsRuntime`의 `contentInstanceId`를 사람이 수동으로 정하던 방식을 제거한다.
- 공용 증가형 ID 발급 로직은 `Foundation`으로 올리고, `ContentsRuntime`는 도메인 인코딩 정책만 담당한다.
- 나중에 다른 오브젝트도 같은 allocator primitive를 재사용할 수 있게 한다.

## 2. 책임 분리
- `Foundation`
  - 범용 증가형 allocator 인터페이스와 기본 구현 제공
  - `content`, `room`, `session` 같은 도메인 의미는 모른다
- `ContentsRuntime`
  - `contentId + reserve + sequence` 비트 정책을 담당
  - `contentId`별로 독립된 sequence allocator를 관리한다
  - 각 `contentId`용 allocator에서 받은 증가값을 `contentInstanceId`로 인코딩한다

## 3. 현재 비트 정책
- `FContentInstanceId`는 `uint64_t`
- 비트 구성
  - `contentId`: 16비트
  - `reserve`: 4비트
  - `sequence`: 44비트
- `0`은 invalid 값으로 예약한다

## 4. reserve 비트 사용 의도
- 현재는 `reserve = 0`만 사용한다
- 하지만 나중에 아래 같은 운영 의미를 넣을 여지를 남긴다
  - `generation/version`
  - `static/dynamic` 같은 인스턴스 종류
  - 기타 운영 구분 비트

## 5. 구현 방향
- `Foundation/Ids`
  - `IIdAllocator`
  - `FDefaultIncrementIdAllocator`
- `ContentsRuntime/Core`
  - `MakeContentInstanceId(...)`
  - `ExtractContentId(...)`
  - `ExtractContentInstanceReserveBits(...)`
  - `ExtractContentInstanceSequence(...)`
  - `FContentInstanceIdAllocator`

`FContentInstanceIdAllocator`는 내부에서 `contentId -> sequence allocator` 맵을 가진다.

## 6. 적용 대상
- `AuthContent`
- `LobbyContent`
- `RoomContent`

샘플 콘텐츠는 모두 런타임 시작 시 allocator를 통해 `contentInstanceId`를 발급받는다.

## 7. 기대 효과
- `ContentsRuntime` 전체에서 전역 unique `contentInstanceId` 보장
- 샘플 서버가 하드코딩된 인스턴스 ID에 의존하지 않음
- 같은 allocator 패턴을 다른 도메인에도 재사용 가능
- `sequence`가 콘텐츠 타입별로 독립적으로 증가하므로 로그 해석이 더 자연스럽다

## 8. 후속 과제
- `reserve` 비트의 실제 의미를 언제 도입할지 운영 정책으로 정리
- 동적 생성/삭제가 필요한 콘텐츠에서 `Release` 재사용 정책이 필요한지 검토
