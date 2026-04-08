# 중복 로그인 정책 계획

상태: 초안  
정본: 예  
최종 갱신: 2026-04-09  
범위: `LoginServer + ChattingServer + WinForms` 중복 로그인 정책  
결론: `신규 로그인 우선`, 기존 세션 강제 종료

## 1. 목표
- 동일한 `loginId / userId`로 새 로그인 시도가 들어오면, 기존 채팅 세션을 끊고 이번 로그인 세션을 받아들이는 정책을 도입한다.
- 사용자가 체감하는 정책은 단순해야 한다.
  - 새로 로그인한 쪽은 정상 진입
  - 이전에 접속 중이던 쪽은 강제 종료
- 현재 `LoginServer HTTP 로그인 -> Redis ticket -> ChattingServer LoginAuthRq` 흐름 위에서 자연스럽게 동작하도록 설계한다.

## 2. 채택 정책

### 2-1. 최종 정책
- 정책 이름:
  - `신규 로그인 우선`
- 동작:
  1. 사용자가 새로 로그인한다.
  2. 새 세션이 `ChattingServer` 인증을 통과한다.
  3. 같은 `userId`로 이미 붙어 있던 기존 세션이 있으면 서버가 그 세션을 끊는다.
  4. 새 세션만 살아남는다.

### 2-2. 이 정책을 고른 이유
- 사용자가 가장 이해하기 쉽다.
- PC와 노트북처럼 다른 기기에서 다시 로그인했을 때 기대 동작과 맞다.
- 운영 입장에서도 “현재 살아 있는 대표 세션 1개” 규칙이 단순하다.
- 이후 친구 목록, 귓속말, 상태 표시 같은 기능을 붙일 때도 기준 세션이 하나라 관리가 쉽다.

## 3. 현재 구조에서의 문제점

### 3-1. `LoginServer`는 중복 로그인 상태를 모른다
- 현재 `LoginServer`는 계정 인증 후 Redis에 ticket만 발급한다.
- 기존에 `ChattingServer`에 붙어 있는 동일 `userId` 세션을 직접 추적하지 않는다.

### 3-2. `ChattingServer`는 사용자 단위 매핑이 없다
- 현재 [FUserRegistry.h](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/Chatting/ChattingServer/Contents/Session/FUserRegistry.h)는 `sessionId -> userId`만 관리한다.
- 즉 “이 `userId`가 이미 어느 `sessionId`로 붙어 있는가”를 바로 찾을 수 없다.

### 3-3. 예전 ticket가 뒤늦게 들어오는 문제를 아직 막지 못한다
- 현재 Redis ticket value는 `userId` 문자열 하나다.
- 이 상태에서는 먼저 발급된 ticket와 나중에 발급된 ticket를 구분할 버전 정보가 없다.
- 따라서 “진짜 최신 로그인만 살린다”는 보장을 강화하려면 ticket payload 확장이 필요하다.

## 4. 권장 구현 방향

### 4-1. 1차 구현 원칙
- 실제 세션 종료 집행은 `ChattingServer`가 맡는다.
- 최신 로그인 판정 정보는 `LoginServer`가 만든다.
- 즉 역할 분리는 아래처럼 가져간다.
  - `LoginServer`
    - 인증 성공
    - 최신 로그인 버전 발급
    - Redis ticket 기록
  - `ChattingServer`
    - ticket consume
    - 최신 버전 확인
    - 기존 세션 disconnect
    - 새 세션 accept

### 4-2. 추천 데이터 모델
- Redis active login key:
  - `chat:active-login:{userId}`
- Redis chat ticket key:
  - `chat:ticket:{ticket}`
- ticket payload 예시:
```json
{
  "userId": 101,
  "loginVersion": 7,
  "nickname": "tester01",
  "issuedAtUtc": "2026-04-09T02:30:00Z"
}
```

### 4-3. 핵심 아이디어
- `LoginServer` 로그인 성공 시 `loginVersion`을 증가시킨다.
- 발급 ticket에도 같은 `loginVersion`을 넣는다.
- `ChattingServer`는 ticket consume 후, Redis의 `chat:active-login:{userId}` 값과 ticket의 `loginVersion`이 같을 때만 통과시킨다.
- 통과 직후 같은 `userId`의 기존 세션이 있으면 `DisconnectSession(previousSessionId)`를 호출한다.
- 이후 새 세션을 registry에 등록한다.

## 5. 단계별 구현 계획

### 5-1. LoginServer 변경
- [chat-ticket-service.ts](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/src/services/chat-ticket-service.ts)
- [auth-types.ts](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/src/models/auth-types.ts)
- [openapi.yaml](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/docs/openapi.yaml)

작업:
1. `userId`별 active login version key를 Redis에 둔다.
2. 로그인 성공 시 version을 증가시킨다.
3. ticket payload를 단순 `userId 문자열`에서 구조화된 payload로 바꾼다.
4. 필요하면 로그인 응답에 `loginVersion`은 내부 추적용으로만 유지하고, WinForms에는 그대로 `ticket`만 넘긴다.

### 5-2. Connector / Redis consume 변경
- [FRedisChatTicketStore.h](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/Libraries/Connector/Redis/FRedisChatTicketStore.h)
- [FRedisChatTicketStore.cpp](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/Libraries/Connector/Redis/FRedisChatTicketStore.cpp)
- [RedisChatTicketStoreTypes.h](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/Libraries/Connector/Config/RedisChatTicketStoreTypes.h)

작업:
1. consume 결과를 `userId`만이 아니라 `loginVersion`, 필요 시 `nickname`까지 담도록 확장한다.
2. Redis value가 JSON이면 파싱한다.
3. 파싱 실패와 version mismatch를 구분된 에러로 남긴다.

### 5-3. ChattingServer 세션 registry 확장
- [FUserRegistry.h](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/Chatting/ChattingServer/Contents/Session/FUserRegistry.h)
- [FUserRegistry.cpp](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/Chatting/ChattingServer/Contents/Session/FUserRegistry.cpp)
- [FAuthContent.cpp](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/Chatting/ChattingServer/Contents/Auth/FAuthContent.cpp)

작업:
1. `sessionId -> userId` 외에 `userId -> sessionId` 조회를 지원한다.
2. 새 로그인 수락 직전에 기존 세션 존재 여부를 찾는다.
3. 기존 세션이 있으면 `bridge.DisconnectSession(previousSessionId)`를 호출한다.
4. 그 다음 새 세션을 최종 대표 세션으로 등록한다.

권장 순서:
1. ticket 검증 성공
2. 기존 세션 조회
3. 기존 세션 disconnect 요청
4. 새 세션 registry 반영
5. lobby 이동
6. `LoginAuthRp success=true`

### 5-4. WinForms 처리
- [MainForm.cs](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/Chatting/ChattingClientWinForms/MainForm.cs)

작업:
1. 내 세션이 중복 로그인으로 끊긴 경우 시스템 메시지를 더 명확히 보여준다.
2. 가능하면 “다른 위치에서 로그인되어 연결이 종료되었습니다.” 문구를 띄운다.
3. 이후 자동 재로그인은 기본값으로 넣지 않고, 우선은 수동 재로그인으로 둔다.

## 6. 추천 정책 세부사항

### 6-1. 언제 기존 세션을 끊을 것인가
- 추천:
  - `ChattingServer LoginAuth` 성공 시점
- 이유:
  - HTTP 로그인만 해놓고 실제 채팅 서버 접속은 안 하는 경우가 있을 수 있다.
  - 따라서 `LoginServer /auth/login` 시점에 곧바로 기존 세션을 끊으면 사용자가 의도치 않게 튕길 수 있다.
  - 실제 새 세션이 채팅 서버에 입장하는 순간 기존 세션을 끊는 편이 더 자연스럽다.

### 6-2. 예전 ticket 처리
- 추천:
  - 최신 `loginVersion`이 아닌 ticket는 인증 실패
- 이유:
  - 오래된 ticket 재사용
  - 먼저 발급받고 나중에 접속하는 순서 꼬임
  - 이 문제를 막아야 “새 로그인 우선” 정책이 진짜로 성립한다.

### 6-3. Room 내 기존 세션 종료 방식
- 추천:
  - 강제 disconnect
- 이유:
  - 콘텐츠 전이보다 구현이 단순하다.
  - room/lobby/auth 어느 위치에 있어도 동일 처리 가능하다.
  - 이미 [IContentBridge.h](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/Libraries/ContentsRuntime/Bridge/IContentBridge.h)에 `DisconnectSession()`이 있다.

## 7. 예외 시나리오

### 7-1. 기존 세션 disconnect 실패
- 새 세션은 기본적으로 로그인 실패로 돌리지 않는다.
- 이유:
  - disconnect 요청이 이미 진행 중일 수 있다.
  - 네트워크 단절 상태의 유령 세션일 수 있다.
- 대신 로그에 남기고, registry 최종 상태를 새 세션 기준으로 정리하는 방향이 낫다.

### 7-2. 동일 사용자가 거의 동시에 두 번 로그인
- 더 큰 `loginVersion`만 최종 승자로 본다.
- 먼저 들어온 세션이 잠깐 붙더라도 이후 더 최신 버전이 들어오면 이전 세션을 끊는다.

### 7-3. 더미 클라이언트 / 레거시 로그인
- 기존 `LoginRq/Rp` 경로는 당분간 유지한다.
- 중복 로그인 정책은 우선 `LoginAuthRq/Rp` 경로에만 적용한다.
- 더미 벤치와 레거시 테스트를 깨지 않기 위한 분리다.

## 8. 검증 계획
1. 사용자 A가 WinForms로 로그인 후 채팅방 입장
2. 같은 계정으로 다른 WinForms 인스턴스에서 다시 로그인
3. 두 번째 세션은 정상 로그인되고 room list까지 진입
4. 첫 번째 세션은 서버 disconnect
5. 첫 번째 세션 UI에 중복 로그인 종료 메시지 표시
6. 첫 번째 세션에서 더 이상 채팅 송신 불가 확인
7. 먼저 발급된 오래된 ticket 재사용 시 로그인 실패 확인

## 9. 이번 계획의 구현 우선순위
1. `LoginServer` ticket payload에 `loginVersion` 추가
2. `Connector Redis ticket store` payload 확장
3. `FUserRegistry`에 `userId -> sessionId` 조회 추가
4. `FAuthContent`에서 기존 세션 disconnect 후 새 세션 accept
5. `WinForms` 중복 로그인 종료 메시지 보강

## 10. 정리
- 채택 정책은 `신규 로그인 우선, 기존 세션 강제 종료`다.
- 실제 집행 위치는 `ChattingServer LoginAuth 성공 시점`이 가장 적절하다.
- 다만 오래된 ticket 역전 문제를 막기 위해 `LoginServer`가 `loginVersion`을 같이 발급하는 구조로 가는 것이 좋다.
- 즉 구현은 `LoginServer에서 최신성 보장`, `ChattingServer에서 기존 세션 종료`의 2단 분리로 가는 것을 권장한다.
