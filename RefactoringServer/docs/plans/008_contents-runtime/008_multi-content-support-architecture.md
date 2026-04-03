# 멀티 콘텐츠 지원 구조 계획

## 1. 목적
- `ContentsRuntime`가 `Auth`, `Lobby`, `Room`, `Chat`, `Battle`처럼 여러 콘텐츠 타입을 동시에 수용할 수 있는 구조를 정리한다.
- 단순히 콘텐츠 종류를 늘리는 수준이 아니라
  - 콘텐츠 타입 간 전이
  - 콘텐츠 타입별 인스턴스 N개
  - 콘텐츠 타입별 스레드 배치
  까지 확장 가능한 기반을 만든다.

## 2. 현재 구조와 한계
- 현재 구조는 `AuthContent`, `EchoContent`처럼 “콘텐츠 타입별 단일 인스턴스”에 가깝다.
- 즉 지금은
  - `contentId -> content instance 1개`
  - `sessionId -> contentId`
  형태로 보는 편이 맞다.
- 이 상태에선 아래 같은 구조가 바로 자연스럽지는 않다.
  - `LobbyContent` 1개
  - `RoomContent` 타입 아래 room 인스턴스 N개
  - `ChatRoomContent` 타입 아래 채팅방 N개

## 3. 목표 모델

### 3.1 용어 분리
- `ContentType`
  - 콘텐츠 종류
  - 예: `Auth`, `Lobby`, `Room`, `ChatRoom`
- `ContentInstance`
  - 같은 콘텐츠 타입의 실제 실행 단위
  - 예: `Lobby#1`, `ChatRoom#1001`, `ChatRoom#1002`
- `ContentThread`
  - 여러 콘텐츠 인스턴스를 소유하고 실행하는 worker thread

### 3.2 라우팅 분리
- 현재
  - `sessionId -> contentId`
- 확장 후
  - `sessionId -> contentInstanceId`
  - `contentInstanceId -> ownerThread`

즉 세션은 더 이상 “콘텐츠 종류”에 붙는 것이 아니라, “콘텐츠 인스턴스”에 붙는다.

## 4. 디렉터리 기준 확장 방향
- `ContentsRuntime/Core`
  - 공용 식별자
    - `ContentTypeId`
    - `ContentInstanceId`
  - 공용 타입
    - `SessionRoute`
    - `ContentInstanceRoute`
  - `IContent`
- `ContentsRuntime/Bridge`
  - `IContentBridge`
- `ContentsRuntime/Routing`
  - `sessionId -> contentInstanceId`
  - `contentInstanceId -> ownerThread`
  - 인스턴스 등록/해제/이동
- `ContentsRuntime/Threading`
  - thread별 inbox
  - thread가 소유한 인스턴스 집합 실행
- 이후 필요 시
  - `ContentsRuntime/Factory`
  - `ContentsRuntime/Instances`
  - `ContentsRuntime/Policies`
  추가 가능

## 5. 추천 구현 방향

### 5.1 1차는 `IContent`를 인스턴스 단위로 유지
- `ChatRoomContent(1001)`
- `ChatRoomContent(1002)`
- 장점
  - 현재 구조와 가장 자연스럽게 이어짐
  - 기존 `FContentThread`, `lock-free inbox`, `route table`을 거의 그대로 재사용 가능
- 단점
  - 인스턴스 수가 아주 많아지면 객체 수가 늘어남

### 5.2 2차에서 타입/인스턴스 분리 검토
- 필요하면 이후
  - `IContentType`
  - `IContentInstance`
  구조로 분리
- 하지만 지금 단계에서는 과하다.

## 6. 패킷 처리 흐름
1. `NetworkLib`가 패킷을 수신한다.
2. 상위 서버가 `ContentsRuntime.EnqueuePacket(sessionId, ...)`를 호출한다.
3. `ContentsRuntime`는 `sessionId -> contentInstanceId`를 찾는다.
4. `contentInstanceId -> ownerThread`를 찾는다.
5. 해당 thread의 `lock-free packet inbox`에 owned packet을 넣는다.
6. thread는 대상 인스턴스를 찾아 `OnPacket(...)`을 호출한다.

핵심은 이겁니다.
- `lock-free packet inbox`는 그대로 재사용 가능
- 바뀌는 것은 라우팅 대상이 `contentId`에서 `contentInstanceId`로 세분화되는 점

## 7. 콘텐츠 전이 흐름
- 현재
  - `MoveSession(sessionId, targetContentId)`
- 확장 후
  - `MoveSessionToInstance(sessionId, targetInstanceId)`
  - 또는 `MoveSession(sessionId, targetTypeId, targetInstanceId)`

예:
1. `LobbyContent`가 `RoomEnterRq`를 받는다.
2. `RoomManager`가 `ChatRoom#1007`을 선택한다.
3. 서버는 세션 라우팅을 `ChatRoom#1007`로 먼저 바꾼다.
4. 그 다음 `RoomEnterRp`를 보낸다.

즉 기존 전이 규칙은 그대로 유지된다.
- “다음 콘텐츠 요청을 열어주는 응답은 전이 완료 후 보낸다”

## 8. 기존 구조와의 호환성

### 8.1 `lock-free packet inbox`
- 그대로 유지 가능하다.
- inbox는 여전히 “어느 thread로 보낼지”까지만 책임지면 된다.
- thread 내부에서 인스턴스 dispatch 한 단계만 추가하면 된다.

### 8.2 route table
- 그대로 쓸 수 있다.
- 단, 값이
  - `contentId`
  대신
  - `contentInstanceId`
  또는 `runtimeSlotId`
  가 되도록 바꾸면 된다.

### 8.3 현재 단일 인스턴스 콘텐츠
- `AuthContent`, `EchoContent`는 그대로 인스턴스 1개로 유지할 수 있다.
- 즉 멀티 콘텐츠 지원을 넣어도 기존 구조를 깨지 않고 호환 가능하다.

## 9. 단계적 구현 순서
1. 공용 식별자 추가
  - `ContentTypeId`
  - `ContentInstanceId`
2. 세션 라우팅을 `sessionId -> contentInstanceId`로 확장
3. `FContentThread`가 여러 인스턴스를 소유할 수 있게 확장
4. 현재 `Auth`, `Echo`를 새 구조에 어댑트
5. 샘플로 `Lobby`, `ChatRoom` 2~3개 인스턴스 추가
6. `Lobby -> Room` 전이 검증

## 10. 검증 기준
- 기존 `Auth -> Echo` 스모크가 그대로 통과
- 기존 `lock-free inbox` 안정성 가정이 깨지지 않음
- `ChatRoom` 인스턴스 2~3개에서
  - 세션별 올바른 room dispatch
  - room 간 전이
  - 세션 종료 시 leave 정리
  가 확인됨

## 11. 결론
- 멀티 콘텐츠 지원은 현재 `ContentsRuntime` 위에 자연스럽게 확장 가능하다.
- 핵심은 “콘텐츠 종류”와 “콘텐츠 인스턴스”를 분리하는 것이다.
- 현재 안정화된
  - `lock-free packet inbox`
  - `route table`
  - 콘텐츠 전이 규칙
  은 그대로 재사용 가능하다.
