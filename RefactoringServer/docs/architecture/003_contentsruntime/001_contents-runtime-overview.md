# ContentsRuntime 개요

## 1. 역할
- `ContentsRuntime`는 콘텐츠 스레드, 콘텐츠 전이, 콘텐츠 라우팅을 담당하는 상위 실행 계층이다.
- `NetworkLib`가 전달한 `sessionId + opcode + payload`를 받아 어떤 콘텐츠 인스턴스가 처리할지 결정한다.

## 2. 왜 NetworkLib 밖에 두는가
- `NetworkLib`는 소켓, 세션, 송수신, 패킷 경계 같은 네트워크 코어에 집중한다.
- 콘텐츠 스레드와 콘텐츠 전이 규칙은 게임 로직 실행 모델에 가깝다.
- 그래서 현재 구조는
  - `NetworkLib`: 네트워크 기반
  - `ContentsRuntime`: 콘텐츠 실행, 이동, 라우팅
  로 분리되어 있다.

## 3. 현재 핵심 구조
- `Core`
  - [ContentRuntimeTypes.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Core\ContentRuntimeTypes.h)
  - [IContent.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Core\IContent.h)
- `Bridge`
  - [IContentBridge.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Bridge\IContentBridge.h)
- `Threading`
  - [FContentThread.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Threading\FContentThread.h)
- `Routing`
  - [FContentRuntime.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Routing\FContentRuntime.h)

## 4. 라우팅 모델
- 초기 구조는 `sessionId -> contentId`에 가까웠다.
- 현재는 멀티 인스턴스 지원을 위해 다음 모델로 확장되었다.
  - `sessionId -> contentInstanceId`
  - `contentInstanceId -> ownerThread`
  - `contentInstanceId -> contentId`
- 기본 인스턴스가 필요한 경우에는 `contentId -> default contentInstanceId` 매핑을 사용한다.

## 5. 패킷 처리 흐름
1. `NetworkLib`가 패킷을 수신한다.
2. 서버 상위 계층이 `ContentsRuntime.EnqueuePacket(...)`를 호출한다.
3. `ContentsRuntime`가 `sessionId`의 현재 `contentInstanceId`를 보고 대상 콘텐츠 스레드를 찾는다.
4. 콘텐츠 스레드는 `Enter / Leave / Packet / Frame` 순서로 큐를 비우며 처리한다.
5. 콘텐츠는 `IContentBridge`를 통해
   - 패킷 전송
   - 세션 이동
   - 세션 종료
   - 현재 콘텐츠 조회
   같은 런타임 기능을 사용한다.

## 6. 로비/룸 멀티 인스턴스 흐름
- 현재 샘플 서버는 다음 흐름을 실제로 사용한다.
  - `LoginRq -> LoginRp -> Lobby`
  - `RoomListRq/Rp`
  - `RoomEnterRq/Rp`
  - `RoomEcho`
  - `RoomListRq/Rp`
  - `RoomChangeRq/Rp`
- 룸 콘텐츠는 같은 타입의 여러 인스턴스로 배치된다.
- 각 룸은 고유한 `roomId`와 `contentInstanceId`를 가진다.

## 7. 전이 규칙
- 다음 콘텐츠 요청을 열어주는 응답이라면 `MoveSession`이 응답 전송보다 먼저 완료되어야 한다.
- 예를 들어 `RoomEnterRp`, `RoomChangeRp` 같은 전이 응답은 새 인스턴스로의 이동이 끝난 뒤 보내야 한다.
- 자세한 규칙은 [003_content-transition-rules.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\003_content-transition-rules.md)를 기준으로 본다.

## 8. lock-free inbox
- 콘텐츠 스레드 packet inbox는 현재 lock-free 프로토타입을 사용한다.
- 장시간 안정성 검증 결과는 [009_contents-runtime-lockfree-validation-result.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\009_contents-runtime-lockfree-validation-result.md)에 정리되어 있다.
- 현재 결론은 `lock-free inbox 유지 가능`이다.

## 9. 관측과 후속 과제
- `FContentRuntime`, `FContentThread`는 queue depth, enter/leave/packet/frame 처리량, lock wait 등을 계측한다.
- 다음 확장 포인트는
  - 룸 흐름 장시간 검증 확대
  - 멀티 콘텐츠 타입 확장
  - 인스턴스 배치 정책 구체화
  이다.
