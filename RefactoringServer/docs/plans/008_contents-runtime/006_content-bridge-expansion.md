# IContentBridge 확장 계획

## 1. 목적
- 콘텐츠 코드가 세션 상태를 최소한으로 조회하거나 제어할 수 있게 브리지 인터페이스를 확장한다.
- 네트워크 구현 세부를 직접 노출하지 않으면서, 콘텐츠 작성 편의성을 높인다.

## 2. 현재 한계
- 초기 `IContentBridge`는 `SendRaw`, `MoveSession`만 제공했다.
- 이 상태로는 콘텐츠가 다음과 같은 기본 질문에 답하기 어렵다.
  - 세션이 아직 살아 있는가?
  - 현재 어떤 콘텐츠에 라우팅되어 있는가?
  - 필요 시 세션을 끊을 수 있는가?

## 3. 확장 대상
- `DisconnectSession(sessionId)`
  - 콘텐츠 정책상 즉시 종료가 필요할 때 사용
- `IsSessionAlive(sessionId)`
  - 지연 처리나 프레임 처리 중 유효성 확인에 사용
- `GetCurrentContentId(sessionId)`
  - 전이 상태 점검, 디버깅, 방어 코드에 사용

## 4. 원칙
- 콘텐츠는 여전히 `NetworkLib` 구현 세부를 알지 않는다.
- 브리지는 얇고 좁게 유지한다.
- 범용적인 세션 제어만 열고, 소켓/세션 내부 구조는 직접 노출하지 않는다.

## 5. 검증 기준
- `IContentBridge` 확장 후 `ContentsRuntime` 빌드 성공
- `FIocpServer`, `FStubServer`가 새 인터페이스를 만족
- `EchoServer`와 `EchoClient` 스모크 성공
