# ContentsRuntime 콘텐츠 전이 규칙

## 1. 목적
- 콘텐츠 전이 시점에 이전 콘텐츠 요청과 다음 콘텐츠 요청이 섞이면서 발생하는 race를 막는다.
- 특히 `Login -> Lobby`, `Lobby -> Room`, `Auth -> Echo`처럼 단계가 바뀌는 구간에서 세션 라우팅 규칙을 명확히 한다.

## 2. 핵심 규칙
- 클라이언트가 `Rp`를 받은 직후 다음 콘텐츠의 `Rq`를 보낼 수 있다면,
  그 `Rp`를 보내기 전에 서버의 콘텐츠 전이는 이미 완료되어 있어야 한다.

## 3. 안전한 순서

### 전이 없는 일반 요청
- 같은 콘텐츠 안에서 끝나는 요청은 일반적인 `Rq -> Rp` 순서로 처리해도 된다.
- 예:
  - `EchoRq -> EchoRp`
  - 같은 Room 안에서의 조회 응답

### 전이를 여는 요청
- 응답을 받은 클라이언트가 바로 다음 콘텐츠 요청을 보낼 수 있는 경우에는 아래 순서를 따른다.
  1. 현재 콘텐츠에서 요청 검증
  2. 세션 라우팅/상태를 다음 콘텐츠로 전이
  3. 전이 완료 후 응답 전송

## 4. 이번 사례에 적용

### 잘못된 순서
1. `LoginRq`
2. `LoginRp` 전송 enqueue
3. `MoveSession`

- 이 순서에서는 `LoginRp`를 받은 클라이언트가 즉시 `RoomSnapshotRq`를 보내면,
  서버 라우트가 아직 `AuthContent`일 수 있다.

### 올바른 순서
1. `LoginRq`
2. `MoveSession`
3. `LoginRp`

- 이 순서에서는 `LoginRp`를 받는 순간 세션이 이미 `EchoContent`로 넘어가 있으므로,
  다음 요청은 항상 새 콘텐츠로 들어간다.

## 5. 적용 대상 예시
- `LoginRq -> LoginRp -> MoveSession`
- `LobbyEnterRq -> LobbyEnterRp -> MoveSession`
- `RoomEnterRq -> RoomEnterRp -> MoveSession`
- `MatchAcceptRq -> MatchAcceptRp -> MoveSession`

## 6. 예외
- 응답을 받아도 다음 콘텐츠 요청을 보낼 수 없는 경우에는 전이를 먼저 할 필요가 없다.
- 예:
  - 같은 콘텐츠 내부 상태 변경 응답
  - 단순 조회 응답
  - 콘텐츠 라우팅과 무관한 ack

## 7. 구현 체크리스트
- 전이 응답인지 먼저 구분한다.
- 전이 응답이라면 `MoveSession` 또는 동등한 상태 전이가 먼저 일어나야 한다.
- `MoveSession` 실패 시 응답을 보내지 말고 실패를 로그로 남긴다.
- 이전 콘텐츠는 다음 콘텐츠 요청을 받았을 때 조용히 무시하지 말고 경고 로그를 남기는 것이 좋다.

## 8. 권장 보조 규칙
- bootstrap이나 콘텐츠 전이 구간은 trace를 켜서 ingress와 handler 진입을 좁게 볼 수 있게 한다.
- 전이 구간에서만 사용하는 전용 로그 문구를 둔다.
- 장시간 soak 결과와 전이 race 분석 결과는 분리해서 기록한다.
