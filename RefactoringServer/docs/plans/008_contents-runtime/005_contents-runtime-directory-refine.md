# ContentsRuntime 디렉터리 세분화 계획

## 1. 목적
- `ContentsRuntime`가 커지기 전에 책임 기준으로 디렉터리를 더 명확하게 나눈다.
- `Core`에 모든 타입이 몰리는 것을 피하고, 이후 `Inbox`, `Routing`, `Stats` 같은 확장을 자연스럽게 받도록 만든다.

## 2. 현재 문제
- 초기 구조는 `Core`, `Bridge` 두 축만으로 시작해서 빠르게 검증하기엔 좋았다.
- 하지만 실제로는
  - 실행 모델
  - 라우팅
  - 통계/관측
  - 큐 구현
  의 책임이 점점 분리되고 있다.

## 3. 목표 구조
- `ContentsRuntime/Core`
  - 순수 공용 타입과 인터페이스
  - `IContent`
  - `ContentRuntimeTypes`
- `ContentsRuntime/Bridge`
  - `IContentBridge`
- `ContentsRuntime/Threading`
  - 콘텐츠 스레드 worker
  - queue drain, frame loop
- `ContentsRuntime/Routing`
  - `sessionId -> contentId` 매핑
  - 콘텐츠 등록/시작/종료
  - packet 라우팅

## 4. 기대 효과
- 파일 위치만 봐도 책임이 드러난다.
- 이후 `Inbox`, `Stats`, `Validation` 같은 축을 추가하기 쉬워진다.
- 문서와 코드의 참조 경로도 더 명확해진다.

## 5. 검증 기준
- `ContentsRuntime` 프로젝트 빌드 성공
- `EchoServer`, `EchoClient` 빌드 성공
- `Login -> Chat snapshot -> Echo` 스모크 성공
