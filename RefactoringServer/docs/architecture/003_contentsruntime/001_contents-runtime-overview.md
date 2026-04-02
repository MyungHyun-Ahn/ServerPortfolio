# ContentsRuntime 개요

## 1. 역할
- `ContentsRuntime`는 콘텐츠 스레드와 콘텐츠 전이 모델을 담당하는 상위 계층이다.
- `NetworkLib`가 넘겨준 `sessionId + packet`을 받아 어떤 콘텐츠가 처리할지 라우팅하고, 프레임 단위로 실행한다.

## 2. 왜 분리했나
- 콘텐츠 스레드를 `NetworkLib` 안에 넣으면 네트워크 코어와 게임 로직 실행 모델의 경계가 흐려진다.
- 레거시의 좋은 실행 패턴은 가져오되, `g_NetServer` 같은 강한 결합은 버리는 방향이 필요했다.
- 그래서 현재 구조는
  - `NetworkLib`: 네트워크 기반
  - `ContentsRuntime`: 콘텐츠 실행/전이
  로 분리되어 있다.

## 3. 구성
- `Core`
  - [ContentRuntimeTypes.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Core\ContentRuntimeTypes.h)
  - [IContent.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Core\IContent.h)
- `Bridge`
  - [IContentBridge.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Bridge\IContentBridge.h)
- `Threading`
  - [FContentThread.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Threading\FContentThread.h)
- `Routing`
  - [FContentRuntime.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Routing\FContentRuntime.h)

## 4. 처리 흐름
1. `NetworkLib`가 패킷을 수신한다.
2. 상위 서버가 `ContentsRuntime.EnqueuePacket(...)`를 호출한다.
3. `ContentsRuntime`는 `sessionId -> contentId`를 보고 대상 콘텐츠 스레드를 찾는다.
4. borrowed view 수명 문제를 피하기 위해 payload는 필요 시 owned buffer로 전환한다.
5. 콘텐츠 스레드는 `Enter / Leave / Packet / Frame` 순서로 큐를 비우며 처리한다.

## 5. 전이 규칙
- 콘텐츠 전이 응답(`LoginRp`처럼 다음 콘텐츠 요청을 열어주는 응답)은 전이 완료 후 보낸다.
- 즉 `MoveSession`이 끝나기 전에 다음 콘텐츠의 `Rq`가 들어오면 이전 콘텐츠로 잘못 라우팅될 수 있다.
- 자세한 규칙은 [003_content-transition-rules.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\003_content-transition-rules.md)를 기준으로 본다.

## 6. 현재 적용 예시
- `AuthContent`
  - `LoginRq` 처리
  - 성공 시 `EchoContent`로 세션 이동
- `EchoContent`
  - `EchoRq`
  - `RoomSnapshotRq`

## 7. 관측과 검증
- `FContentThread`, `FContentRuntime`에는 기본 계측이 들어가 있다.
- queue depth, enter/leave/packet/frame 처리량, delay frame 등을 확인할 수 있다.
- lock-free packet inbox는 현재 프로토타입 단계이며, race injection 기반으로 안정성 검증 중이다.
