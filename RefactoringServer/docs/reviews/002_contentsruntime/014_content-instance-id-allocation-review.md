# Content Instance ID 할당 리뷰

## 1. 변경 배경
- 기존 샘플은 `1001`, `2001`, `3001` 같은 하드코딩된 `contentInstanceId`를 사용했다.
- 이 방식은 샘플 단계에서는 단순하지만, 콘텐츠 타입과 인스턴스 수가 늘면 충돌 방지와 운영 규칙을 사람이 계속 관리해야 한다.
- 현재 구조에서는 `contentInstanceId`가 `ContentsRuntime` 전체에서 전역 unique 키이기 때문에, 수동 번호 체계는 오래 유지하기 어렵다.

## 2. 현재 코드 구조
- 공용 증가값 allocator primitive
  - [IIdAllocator.h](D:\Project\ServerPortfolio\RefactoringServer\Foundation\Ids\IIdAllocator.h)
  - [FDefaultIncrementIdAllocator.h](D:\Project\ServerPortfolio\RefactoringServer\Foundation\Ids\FDefaultIncrementIdAllocator.h)
  - [FDefaultIncrementIdAllocator.cpp](D:\Project\ServerPortfolio\RefactoringServer\Foundation\Ids\FDefaultIncrementIdAllocator.cpp)
- `ContentsRuntime` 도메인 wrapper
  - [FContentInstanceIdAllocator.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Core\FContentInstanceIdAllocator.h)
  - [FContentInstanceIdAllocator.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Core\FContentInstanceIdAllocator.cpp)
- 비트 정책과 해석 helper
  - [ContentRuntimeTypes.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Core\ContentRuntimeTypes.h)
- 실제 사용 예
  - [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)
  - [FAuthContent.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Contents\Auth\FAuthContent.cpp)
  - [FLobbyContent.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Contents\Lobby\FLobbyContent.cpp)
  - [FEchoContent.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Contents\Echo\FEchoContent.cpp)

## 3. 책임 분리
- `Foundation/Ids`
  - 증가값을 발급하는 범용 primitive만 담당한다.
  - `content`, `room`, `session`, `server` 같은 도메인 의미는 모른다.
- `ContentsRuntime/Core`
  - `contentId + reserve + sequence`를 실제 `contentInstanceId`로 인코딩한다.
  - 내부적으로 `contentId`별 독립 sequence allocator를 관리한다.
  - `contentInstanceId`를 다시 `contentId`, `reserve`, `sequence`로 해석하는 helper를 제공한다.

즉 증가값 발급은 공용화하고, 도메인 규칙은 `ContentsRuntime`에 둔 구조다.

## 4. 비트 레이아웃
- `FContentInstanceId`는 `uint64_t`
- 현재 비트 구성
  - `contentId`: 16비트
  - `reserve`: 4비트
  - `sequence`: 44비트
- `0`은 invalid 값으로 예약한다.

핵심 helper:
- `MakeContentInstanceId(...)`
- `ExtractContentId(...)`
- `ExtractContentInstanceReserveBits(...)`
- `ExtractContentInstanceSequence(...)`
- `IsValidContentInstanceId(...)`

## 5. 사용 흐름
1. 상위 계층이 `FContentInstanceIdAllocator`를 만든다.
2. `Allocate(contentId, reserveBits)`가 호출되면 해당 `contentId`용 sequence allocator를 찾거나 새로 만든다.
3. 그 allocator에서 증가값을 받아 `contentInstanceId`를 만든다.
4. 생성된 ID를 콘텐츠 생성자에 넘긴다.
5. 콘텐츠는 `GetContentInstanceId()`에서 그 값을 그대로 반환한다.
6. `FContentRuntime`는 이 값을 전역 key로 등록하고 라우팅에 사용한다.

샘플 서버에서는 [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)에서
- `AuthContent`
- `LobbyContent`
- 각 `RoomContent`

생성 전에 allocator로 ID를 먼저 발급하고, 그 ID를 콘텐츠 생성자에 주입한다.

## 6. 샘플 기준 사용 예
- `AuthContent`
  - [FAuthContent.h](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Contents\Auth\FAuthContent.h)
  - 생성자에서 `contentInstanceId`를 받는다.
  - `GetContentInstanceId()`는 하드코딩 상수가 아니라 멤버 `m_contentInstanceId`를 돌려준다.
- `LobbyContent`
  - [FLobbyContent.h](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Contents\Lobby\FLobbyContent.h)
  - `Auth`와 같은 패턴으로 주입받은 ID를 사용한다.
- `RoomContent`
  - [FEchoContent.h](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Contents\Echo\FEchoContent.h)
  - 기존에도 생성자 주입이었고, 이제도 같은 방식으로 allocator 결과를 받는다.

## 7. 주의사항

### 7.1 allocator 인스턴스를 논리적 allocation domain마다 하나로 관리해야 한다
- 가장 중요한 주의사항이다.
- `FContentInstanceIdAllocator`를 기본 생성하면 내부에서 `contentId`별 `FDefaultIncrementIdAllocator`를 필요할 때마다 만든다.
- 같은 래퍼 인스턴스 안에서는 `contentId`별 sequence가 독립적으로 증가한다.
- 하지만 서로 다른 `FContentInstanceIdAllocator` 인스턴스가 같은 `contentId`에 대해 동시에 발급하면 같은 `sequence`를 만들 수 있다.

즉 한 allocation domain 안에서 같은 `contentId`의 `contentInstanceId`를 발급할 때는
- 같은 `FContentInstanceIdAllocator` 인스턴스를 공유하는 것이 안전하다.

샘플 서버가 [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)에서 allocator 하나만 만들고 `Auth/Lobby/Room`에 모두 쓰는 이유가 이것이다.

### 7.2 `Release()`를 호출한다고 ID가 재사용되는 것은 아니다
- 현재 기본 구현 [FDefaultIncrementIdAllocator.cpp](D:\Project\ServerPortfolio\RefactoringServer\Foundation\Ids\FDefaultIncrementIdAllocator.cpp)의 `Release()`는 no-op이다.
- 즉 현재 정책은 “단순 증가, 재사용 없음”이다.
- ID를 회수하면 바로 다시 쓰일 거라고 기대하면 안 된다.

### 7.3 `IContent` 기본 구현은 invalid를 돌려준다
- [IContent.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Core\IContent.h)
- 현재 `GetContentInstanceId()` 기본 구현은 `kInvalidContentInstanceId`를 돌려준다.
- 그래서 실제 콘텐츠 타입은
  - 생성자에서 ID를 받아 저장하고
  - `GetContentInstanceId()`를 override해서
  - 유효한 값을 돌려줘야 한다.

이걸 빼먹으면 `FContentRuntime::RegisterContent()`에서 등록이 실패한다.

### 7.4 `contentInstanceId`는 `ContentsRuntime` 전체에서 전역 unique 키다
- `FContentRuntime`는 `contentSlots[contentInstanceId]` 형태로 저장한다.
- 즉 같은 `contentId` 안에서만 unique면 되는 게 아니라, 모든 콘텐츠 타입을 통틀어 unique해야 한다.
- 현재는 `contentId + sequence` 결합 정책 덕분에, sequence가 콘텐츠 타입별로 따로 증가해도 최종 `contentInstanceId`는 전역 unique가 된다.

### 7.5 `reserve`는 아직 의미를 고정하지 않았다
- 현재는 `0`만 사용한다.
- 나중에 분산 서버 구조가 들어오면 `serverId` 같은 의미로 확장할 수 있도록 남겨둔 비트다.
- 지금 단계에서 운영 의미를 섣불리 박아 넣지 않는 것이 맞다.

## 8. 왜 상속보다 wrapper/합성인가
- `Foundation`의 기본 allocator는 “증가값 발급”만 책임져야 한다.
- `ContentsRuntime`의 `contentId`, `reserve`, `sequence` 조합은 도메인 정책이다.
- 그래서
  - `Foundation` 기본 구현을 상속해서 바로 `contentInstanceId`를 만드는 구조보다
  - `FContentInstanceIdAllocator`가 내부에 `IIdAllocator`를 들고 조합하는 구조가 더 명확하다.

이렇게 하면 나중에
- 다른 도메인이
- 같은 증가형 allocator primitive를 재사용할 수 있다.

## 9. 현재 결론
- 증가값 발급 primitive는 `Foundation`
- 도메인 인코딩 정책은 `ContentsRuntime`
- 샘플 콘텐츠는 모두 생성자 주입으로 `contentInstanceId`를 받는다
- 기본 allocator는 현재 “단순 증가, 재사용 없음” 정책이다
- sequence는 현재 `contentId`별로 독립적으로 증가한다

이 구조가 지금 단계에서 가장 안전하고, 나중에 분산 서버나 동적 인스턴스 정책으로 확장하기도 좋다.
