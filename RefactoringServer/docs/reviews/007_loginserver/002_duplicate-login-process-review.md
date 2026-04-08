# 중복 로그인 프로세스 코드 리뷰

## 1. 문서 목적
- [LoginServer](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer)와 [ChattingServer](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/Chatting/ChattingServer) 사이에 추가한 중복 로그인 정책 구현을 코드 기준으로 정리한다.
- 현재 정책인 `신규 로그인 우선, 기존 세션 강제 종료`가 실제로 어떤 코드 경로에서 집행되는지 기록한다.
- 이미 해결한 버그와 아직 남아 있는 운영 리스크를 같이 남겨 다음 후속 작업의 기준으로 쓴다.

## 2. 리뷰 대상
- [chat-ticket-service.ts](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/src/services/chat-ticket-service.ts)
- [IChatTicketStore.h](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/Libraries/Connector/Interfaces/IChatTicketStore.h)
- [RedisChatTicketStoreTypes.h](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/Libraries/Connector/Config/RedisChatTicketStoreTypes.h)
- [FRedisChatTicketStore.h](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/Libraries/Connector/Redis/FRedisChatTicketStore.h)
- [FRedisChatTicketStore.cpp](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/Libraries/Connector/Redis/FRedisChatTicketStore.cpp)
- [FUserRegistry.h](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/Chatting/ChattingServer/Contents/Session/FUserRegistry.h)
- [FUserRegistry.cpp](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/Chatting/ChattingServer/Contents/Session/FUserRegistry.cpp)
- [FAuthContent.cpp](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/Chatting/ChattingServer/Contents/Auth/FAuthContent.cpp)
- [MainForm.cs](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/Chatting/ChattingClientWinForms/MainForm.cs)
- [003_duplicate-login-kick-previous-session-plan.md](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/docs/plans/012_login-platform/003_duplicate-login-kick-previous-session-plan.md)

## 3. 결론
- 현재 구현은 `새 로그인 ticket만 유효`, `기존 세션 강제 종료`, `기존 클라이언트에 종료 징후 표시`까지 의도한 정책대로 동작한다.
- 실제 수동 확인에서도 동일 계정으로 두 번째 로그인 후 첫 번째 `WinForms` 클라이언트가 서버에 의해 종료되는 흐름을 확인했다.
- 다만 `DisconnectSession()` 실패를 로그인 실패로 승격하지 않는 구조라서, 드물게 짧은 중복 접속 구간이 생길 가능성은 남아 있다.

## 4. 현재 처리 흐름

### 4-1. LoginServer
1. [chat-ticket-service.ts](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/src/services/chat-ticket-service.ts)에서 로그인 성공 시 `chat:active-login:{userId}`를 `INCR`한다.
2. 증가된 값을 `loginVersion`으로 사용한다.
3. Redis `chat:ticket:{ticket}`에는 `userId:loginVersion` 문자열을 TTL과 함께 저장한다.

### 4-2. ChattingServer Redis consume
1. [FRedisChatTicketStore.cpp](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/Libraries/Connector/Redis/FRedisChatTicketStore.cpp)에서 `GETDEL chat:ticket:{ticket}`로 ticket를 1회성으로 소비한다.
2. ticket payload를 `userId:loginVersion` 형식으로 파싱한다.
3. `chat:active-login:{userId}` 현재 값을 다시 읽는다.
4. active login version과 ticket version이 다르면 오래된 ticket로 보고 인증을 거부한다.

### 4-3. ChattingServer LoginAuth
1. [FAuthContent.cpp](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/Chatting/ChattingServer/Contents/Auth/FAuthContent.cpp)에서 `LoginAuthRq`를 받는다.
2. connector를 통해 ticket consume 및 최신성 검증을 수행한다.
3. [FUserRegistry.cpp](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/Chatting/ChattingServer/Contents/Session/FUserRegistry.cpp)에서 `userId -> sessionId` 역방향 조회로 기존 세션을 찾는다.
4. 새 세션을 `Lobby`로 이동시킨다.
5. 새 세션을 `FUserRegistry`에 최종 세션으로 반영한다.
6. 기존 세션이 살아 있으면 `DisconnectSession(previousSessionId)`를 호출한다.
7. 새 세션에는 `LoginAuthRp success=true`를 보낸다.

### 4-4. WinForms
1. [MainForm.cs](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/Chatting/ChattingClientWinForms/MainForm.cs)에서 `LoginServer` HTTP 로그인 후 ticket를 받는다.
2. 이후 `ChattingServer`에 `LoginAuthRq`를 전송한다.
3. 기존 세션이 중복 로그인으로 끊기면 시스템 메시지로 교체 가능성을 보여준다.

## 5. 확인된 강점
- `LoginServer`와 `ChattingServer`의 책임이 깔끔하게 분리되어 있다.
- 오래된 ticket 재사용을 `loginVersion` 비교로 차단해서 단순한 중복 로그인뿐 아니라 늦게 도착한 예전 로그인도 막는다.
- `GETDEL`을 사용해 ticket 1회성 보장이 분명하다.
- `FUserRegistry`에 `userId -> sessionId` 조회를 추가하면서 중복 로그인 정책이 room/lobby 위치와 무관하게 동작하게 됐다.
- `WinForms`에서 종료 원인을 유추할 수 있게 메시지를 남겨 사용자 경험도 최소한 보강했다.

## 6. 이번 구현에서 실제로 발견된 이슈

### 6-1. 수정 완료: 기존 세션 조회 전에 registry를 덮어쓰던 문제
- 초기 구현에서는 [FAuthContent.cpp](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/Chatting/ChattingServer/Contents/Auth/FAuthContent.cpp)에서 새 세션을 먼저 `UpsertUser()`한 뒤 기존 세션을 조회하고 있었다.
- 이 순서에서는 `GetSessionId(userId)` 결과가 항상 방금 로그인한 세션으로 바뀌어서 기존 세션 종료가 실행되지 않았다.
- 현재는 `previousSessionId`를 먼저 읽고, 그 뒤에 새 세션을 registry에 반영하도록 수정되어 정상 동작한다.

## 7. 잔여 리스크와 후속 검토 포인트

### 7-1. `DisconnectSession()` 실패를 성공 응답과 분리하고 있다
- [FAuthContent.cpp](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/Chatting/ChattingServer/Contents/Auth/FAuthContent.cpp)에서 기존 세션 종료가 실패해도 새 로그인 자체는 성공 처리한다.
- 현재 정책상 신규 로그인 우선에는 맞지만, 네트워크 레벨 종료가 실패하면 짧은 시간 동안 두 세션이 같이 살아 있을 수 있다.
- 운영 단계에서는 이 경우 추가 로그, 모니터링, 또는 후속 정리 작업이 있으면 더 안전하다.

### 7-2. `legacy LoginRq/Rp` 경로는 중복 로그인 정책 밖에 있다
- 현재 강제 종료 정책은 `LoginAuthRq/Rp` 경로에만 적용되어 있다.
- 기존 더미/레거시 경로를 유지하기 위한 선택으로는 맞지만, 장기적으로는 운영 경로를 `LoginAuth`로 수렴시켜야 정책 일관성이 생긴다.

### 7-3. active login version은 누적 증가만 한다
- [chat-ticket-service.ts](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/src/services/chat-ticket-service.ts)의 `chat:active-login:{userId}`는 계속 증가한다.
- 실사용에는 큰 문제는 없지만, 키 보존 기간과 초기화 정책을 한 번 정해두면 운영 문서가 더 명확해진다.

## 8. 수동 검증 결과
- 동일 계정으로 첫 번째 `WinForms` 클라이언트 로그인 성공
- 동일 계정으로 두 번째 `WinForms` 클라이언트 로그인 성공
- 첫 번째 클라이언트에서 `Connection closed by server.`
- 이어서 `ChattingServer connection closed. If the same account logged in elsewhere, this session may have been replaced.` 메시지 확인
- 위 흐름으로 현재 정책은 의도대로 동작한다고 판단한다.

## 9. 추천 후속 작업
1. `DisconnectSession` 실패 로그를 별도 카운터나 경고 지표로 모니터링한다.
2. `legacy LoginRq/Rp` 축소 계획을 잡아 운영 경로를 `LoginAuth`로 단일화한다.
3. 중복 로그인으로 끊긴 `WinForms` 클라이언트에 한국어 팝업을 추가해 종료 이유를 더 명확하게 안내한다.

## 10. 요약
- 중복 로그인 정책 구현은 현재 기준으로 논리와 실제 동작이 맞는다.
- 핵심은 `LoginServer`의 `loginVersion` 발급과 `ChattingServer`의 기존 세션 종료 집행이다.
- 이번에 한 번 드러난 `registry 반영 순서` 문제까지 수정된 상태라서, 현재 프로세스는 프로토타입 단계 기준으로 충분히 안정적이다.
