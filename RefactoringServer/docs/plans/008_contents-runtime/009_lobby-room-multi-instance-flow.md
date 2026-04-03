# Lobby-Room 멀티 인스턴스 검증 흐름 계획

## 1. 목적
- 현재 `ContentsRuntime`의 `contentId + contentInstanceId` 기반 라우팅을 실제 콘텐츠 흐름으로 검증한다.
- 단순 `Auth -> Echo`를 넘어서 아래 흐름을 실제 구현 대상으로 잡는다.
  - `Login -> Lobby -> RoomList -> RoomEnter -> RoomEcho`
  - `RoomEcho` 상태에서 `RoomList -> RoomChange`

## 2. 핵심 검증 목표
- 로그인 이후 로비 전이가 규칙대로 동작하는지 확인한다.
- 로비에서 받은 룸 목록을 기준으로 유효한 룸 하나를 선택해 입장할 수 있어야 한다.
- 룸은 같은 콘텐츠 타입의 인스턴스 N개를 동시에 지원해야 한다.
- `RoomChangeRq`는 반드시 최신 `RoomListRp` 기준의 유효한 방만 대상으로 해야 한다.
- 같은 룸으로의 `RoomChangeRq`는 거부되어야 한다.
- 존재하지 않는 `contentInstanceId` 또는 더 이상 유효하지 않은 `roomId`를 대상으로 요청이 들어오면 서버가 실패 응답을 내려줄 수 있어야 한다.
- 기존 `lock-free packet inbox`와 세션 라우팅 구조가 이 흐름에서도 깨지지 않아야 한다.

## 3. 콘텐츠 구조

### 3.1 콘텐츠 타입
- `AuthContent`
  - 로그인 전용
- `LobbyContent`
  - 룸 목록 제공
  - 룸 입장 요청 처리
- `RoomContent`
  - 실제 룸 인스턴스
  - 룸 echo 처리
  - 룸 변경 처리

### 3.2 콘텐츠 인스턴스
- `AuthContent`
  - 기본 1개 인스턴스
- `LobbyContent`
  - 기본 1개 인스턴스
- `RoomContent`
  - `roomCount` 만큼 여러 인스턴스 생성
  - 예: `Room#3001`, `Room#3002`, `Room#3003`

## 4. 패킷 구성

### 4.1 Login
- `LoginRq`
  - 기존 유지
- `LoginRp`
  - 기존 유지

### 4.2 Lobby
- `RoomListRq`
  - 로비에서 룸 목록 요청
  - `RoomChange` 전에 최신 룸 정보를 다시 가져오는 용도로도 사용
- `RoomListRp`
  - 방 목록 응답
  - 최소 포함 정보
    - `roomId`
    - `roomName`
    - `participantCount`
    - `isJoinable`
- `RoomEnterRq`
  - 선택한 룸 입장 요청
  - 포함 정보
    - `roomId`
- `RoomEnterRp`
  - 룸 입장 결과
  - 포함 정보
    - `roomId`
    - `success`
    - `resultCode`

### 4.3 Room
- `RoomEchoRq`
  - 룸 상태에서의 echo 요청
- `RoomEchoRp`
  - 룸 상태에서의 echo 응답
- `RoomChangeRq`
  - 다른 룸으로 이동 요청
  - 포함 정보
    - `targetRoomId`
- `RoomChangeRp`
  - 룸 변경 결과
  - 포함 정보
    - `previousRoomId`
    - `currentRoomId`
    - `success`
    - `resultCode`

## 4.4 ResultCode 정책

### 4.4.1 공통 원칙
- `success=false`만으로는 부족하므로 `RoomEnterRp`, `RoomChangeRp`에는 반드시 `resultCode`를 포함한다.
- `resultCode`는 아래 두 축을 구분해야 한다.
  - 정상 상황에서 발생할 수 있는 거부
  - 비정상 상황 또는 서버 오류

### 4.4.2 예시 ResultCode
- `Success`
- `InvalidRoomId`
  - 클라이언트가 보낸 `roomId`가 최신 `RoomList` 기준으로 유효하지 않음
- `SameRoomNotAllowed`
  - 현재 룸과 같은 룸으로 이동 요청
- `RoomFull`
  - 입장 인원 제한 초과
- `RetryRequired`
  - 현재 목록은 유효했지만 순간적인 상태 변화로 재시도가 필요
- `MissingContentInstance`
  - `roomId`는 존재하지만 매핑된 `contentInstanceId`를 찾지 못함
- `RuntimeRouteFailure`
  - 런타임 전이 실패
- `InternalError`
  - 그 외 비정상 오류

### 4.4.3 정상 실패와 비정상 실패 구분
- 정상 실패
  - `InvalidRoomId`
  - `SameRoomNotAllowed`
  - `RoomFull`
  - `RetryRequired`
- 비정상 실패
  - `MissingContentInstance`
  - `RuntimeRouteFailure`
  - `InternalError`

정책:
- 정상 실패는 서비스 흐름 안에서 예상 가능한 결과다.
- 비정상 실패는 관측과 후속 조사 대상이다.

## 5. 상세 패킷 흐름

### 5.1 연결 직후
1. 클라이언트 연결
2. 서버는 세션을 `AuthContent` 기본 인스턴스에 넣는다.
3. 클라이언트는 `LoginRq` 전송

### 5.2 로그인 성공 후 로비 전이
1. `AuthContent`가 `LoginRq` 처리
2. 로그인 성공 시 서버는 먼저 `LobbyContent` 기본 인스턴스로 `MoveSession`
3. 그 다음 `LoginRp(success=true)` 전송

전이 규칙:
- `LoginRp`를 받은 직후 클라이언트가 `RoomListRq`를 보낼 수 있으므로, `LoginRp` 전송 전에 `MoveSession`이 완료되어 있어야 한다.

### 5.3 로비에서 룸 목록 요청
1. 클라이언트는 `LoginRp` 수신 후 `RoomListRq` 전송
2. `LobbyContent`는 현재 생성된 룸 인스턴스 목록을 기반으로 `RoomListRp` 전송
3. 클라이언트는 `RoomListRp`를 저장하고, 그 목록 안에서만 다음 룸 선택을 수행한다.

### 5.4 로비에서 랜덤 룸 입장
1. 클라이언트는 최신 `RoomListRp` 안의 join 가능한 룸 중 하나를 랜덤 선택
2. `RoomEnterRq(roomId)` 전송
3. `LobbyContent`는 `roomId -> contentInstanceId`를 찾아 대상 룸 인스턴스 결정
4. 서버는 먼저 해당 `RoomContent` 인스턴스로 `MoveSessionToInstance`
5. 그 다음 `RoomEnterRp(success=true)` 전송

실패 규칙:
- `roomId`가 목록에 없으면 `RoomEnterRp(success=false)`를 보낸다.
- `roomId`는 유효하지만 매핑된 `contentInstanceId`가 없거나 런타임에서 해당 인스턴스를 찾지 못하면 `RoomEnterRp(success=false)`를 보낸다.
- `roomId`가 유효하지만 현재 입장 인원 제한을 초과하면 `RoomEnterRp(success=false, resultCode=RoomFull)`를 보낸다.

로그 규칙:
- `RoomFull`은 정상 상황이므로 에러 로그로 남기지 않는다.
- `MissingContentInstance`, `RuntimeRouteFailure`, `InternalError`는 비정상 상황이므로 에러 로그를 남긴다.
- 정상 실패는 필요하면 `Info` 또는 `Warn` 수준의 구조화 로그로만 남긴다.

전이 규칙:
- `RoomEnterRp`를 받은 직후 클라이언트가 `RoomEchoRq`나 `RoomChangeRq`를 보낼 수 있으므로, `RoomEnterRp` 전에 반드시 룸 인스턴스로 이동이 끝나 있어야 한다.

### 5.5 RoomEcho 동작
1. 클라이언트가 `RoomEchoRq` 전송
2. 현재 배정된 `RoomContent` 인스턴스가 처리
3. `RoomEchoRp` 전송

검증 포인트:
- 서로 다른 세션이 서로 다른 룸 인스턴스로 분산될 수 있어야 한다.
- 로그에서 `contentInstanceId`와 `roomId`가 일치해야 한다.

### 5.6 RoomChange 동작
1. 클라이언트는 `RoomEcho` 상태에서 일정 확률로 먼저 `RoomListRq`를 다시 전송한다.
2. 서버는 최신 `RoomListRp`를 내려준다.
3. 클라이언트는 그 목록을 기준으로 현재 룸이 아닌 다른 유효한 룸을 랜덤 선택한다.
4. 클라이언트는 `RoomChangeRq(targetRoomId)` 전송
5. `RoomContent`는 아래를 검사한다.
   - `targetRoomId`가 최신 목록 기준으로 유효한지
   - `targetRoomId != currentRoomId` 인지
6. 유효하면 서버는 먼저 대상 `RoomContent` 인스턴스로 `MoveSessionToInstance`
7. 그 다음 `RoomChangeRp(previousRoomId, currentRoomId, success=true)` 전송

거부 규칙:
- 같은 룸으로의 `RoomChangeRq`는 거부
- 최신 `RoomListRp`에 없는 `roomId`를 대상으로 한 `RoomChangeRq`도 거부
- `roomId`는 유효하지만 `contentInstanceId` 매핑이 없거나, 런타임에서 해당 인스턴스를 찾지 못하면 거부
- 대상 룸의 입장 인원 제한이 초과된 경우도 거부
- 위 경우 모두 `MoveSessionToInstance`를 호출하지 않고 `RoomChangeRp(success=false)`를 보낸다.

ResultCode 규칙:
- 같은 룸이면 `SameRoomNotAllowed`
- 최신 목록 기준 유효하지 않으면 `InvalidRoomId`
- 입장 인원 제한 초과면 `RoomFull`
- 인스턴스 매핑 누락이면 `MissingContentInstance`
- 런타임 이동 실패면 `RuntimeRouteFailure`

전이 규칙:
- `RoomChangeRp(success=true)`는 다음 룸의 요청을 바로 열어주는 응답이므로, 응답 전에 전이가 완료되어 있어야 한다.

## 6. 클라이언트 테스트 시나리오

### 6.1 기본 흐름
1. 연결
2. `LoginRq`
3. `LoginRp`
4. `RoomListRq`
5. `RoomListRp`
6. 랜덤 룸 선택
7. `RoomEnterRq`
8. `RoomEnterRp`
9. `RoomEchoRq/Rp`

### 6.2 장시간 검증 흐름
1. 연결 후 위 기본 흐름 완료
2. `RoomContent` 상태에서 반복
   - 일정 간격으로 `RoomEchoRq`
   - 일정 확률로 `RoomListRq -> RoomListRp -> RoomChangeRq`
3. `holdSeconds`가 끝나면 정상 종료

### 6.3 실패 응답에 대한 클라이언트 처리
- 정상 실패 응답을 받은 경우
  - `InvalidRoomId`
  - `SameRoomNotAllowed`
  - `RoomFull`
  - `RetryRequired`
  클라이언트는 이를 비정상 종료로 보지 않는다.
- 위 정상 실패 중 입장 관련 실패를 받으면 클라이언트는 최신 `RoomListRq`를 다시 보내고, 다른 유효 룸으로 재시도한다.
- `RoomFull`은 특히 재시도 대상이다.
- 비정상 실패 응답을 받은 경우
  - `MissingContentInstance`
  - `RuntimeRouteFailure`
  - `InternalError`
  클라이언트는 즉시 테스트 실패로 기록하고 세션을 종료한다.

### 6.4 랜덤 규칙
- `RoomList`에서 랜덤한 룸 하나 선택
- `RoomChange`도 반드시 최신 `RoomListRp` 기준으로 현재 룸을 제외한 룸 중 랜덤 선택
- 룸 수가 1개뿐이면 `RoomChange`는 비활성 또는 항상 실패 처리

## 7. 서버 구현 순서
1. `Lobby` / `Room` 관련 패킷 스키마 추가
2. `PacketGenerator` 재생성
3. `LobbyContent` 추가
4. `RoomContent`를 인스턴스 기반으로 생성하도록 변경
5. `roomId -> contentInstanceId` 매핑 추가
6. 존재하지 않는 `roomId` 또는 존재하지 않는 `contentInstanceId` 대상 요청에 대해 실패 응답을 생성하는 경로 추가
7. 룸별 입장 인원 제한과 현재 인원 계수 추가
8. `ResultCode` 정의 및 서버 로그 정책 추가
9. `EchoClient`에 `RoomList -> RoomEnter -> RoomList -> RoomChange` 흐름과 정상 실패 재시도 규칙 추가
10. 기본 스모크
11. 다중 룸 인스턴스 스모크
12. 중간 길이 soak
13. 장시간 검증

## 8. 테스트 단계

### 8.1 1차 기능 검증
- `sessions=1`
- `roomCount=3`
- `holdSeconds=0`
- 목표
  - 기본 전이 흐름 확인
  - 같은 룸 이동 거부 확인
  - 존재하지 않는 `roomId` 대상 실패 응답 확인
  - 존재하지 않는 `contentInstanceId` 대상 실패 응답 확인
  - `RoomFull` 정상 실패와 클라이언트 재시도 확인

### 8.2 2차 멀티 인스턴스 검증
- `sessions=10~30`
- `roomCount=3~5`
- `holdSeconds=60`
- 목표
  - 세션이 여러 룸 인스턴스로 분산되는지
  - `RoomChange` 후에도 패킷이 새 인스턴스로 정확히 들어가는지

### 8.3 3차 안정성 검증
- `sessions=100`
- `roomCount=5` 이상
- `race injection on`
- `holdSeconds=1800` 또는 그 이상
- 목표
  - `lock-free packet inbox` 유지 상태에서 전이/변경/종료 경로 안정성 확인

## 9. 로그 및 계측 포인트
- 서버
  - `login succeeded`
  - `moved to lobby`
  - `room list served`
  - `room enter succeeded`
  - `room change succeeded/failed`
  - `room enter failed with resultCode`
  - `room change failed with resultCode`
  - `invalid room target`
  - `missing content instance for room`
  - `room full`
  - `room content enter/leave`
  - `contentInstanceId`
  - `roomId`
- 클라이언트
  - `login stage complete`
  - `room list received`
  - `room change candidate list received`
  - `room enter complete`
  - `room change complete`
  - `room enter retry scheduled`
  - `room change retry scheduled`
  - 현재 `roomId`

## 10. 리스크
- 전이 응답 전에 `MoveSession`이 끝나지 않으면 기존 bootstrap race가 다시 발생할 수 있다.
- `RoomChange`가 잦아지면 전이 경합이 집중될 수 있다.
- 룸 목록과 실제 룸 인스턴스 상태가 어긋나면 잘못된 `roomId` 선택이 발생할 수 있다.
- 클라이언트가 오래된 `RoomListRp`를 들고 있으면 이미 제거된 룸을 대상으로 `RoomChangeRq`를 보낼 수 있으므로, 서버는 이를 실패 응답으로 안전하게 처리해야 한다.
- 정상 실패와 비정상 실패를 구분하지 않으면 클라이언트 재시도 정책과 서버 에러 로그 정책이 뒤엉킬 수 있다.

## 11. 결론
- 멀티 콘텐츠 인스턴스 구조를 실제로 검증하려면 `Lobby -> RoomList -> RoomEnter -> RoomChange -> RoomEcho` 흐름이 가장 적합하다.
- 이 흐름은
  - 멀티 콘텐츠 타입
  - 멀티 인스턴스 라우팅
  - 전이 규칙
  - lock-free inbox 안정성
를 한 번에 확인할 수 있다.
