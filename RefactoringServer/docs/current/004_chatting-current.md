# 004 Chatting Current

Status: Active  
Canonical: Yes  
Last Updated: 2026-04-08  
Scope: ChattingServer, ClientNetworkLib, ChattingDummy current state

## 1. 현재 프로젝트 구성
- 서버: [ChattingServer](D:\Project\ServerPortfolio\RefactoringServer\Chatting\ChattingServer)
- 더미 부하 클라이언트: [ChattingDummyClient](D:\Project\ServerPortfolio\RefactoringServer\Chatting\ChattingDummyClient)
- 사용자 프로토타입 클라이언트: [ChattingClientWinForms](D:\Project\ServerPortfolio\RefactoringServer\Chatting\ChattingClientWinForms)
- 공용 클라이언트 네트워크 라이브러리: [ClientNetworkLib](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ClientNetworkLib)

## 2. 현재 packet surface
- `LoginRq / LoginRp`
- `RoomListRq / RoomListRp`
- `RoomChangeRq / RoomChangeRp`
- `ChattingRq / ChattingRp`
- `Broadcast`

## 3. 현재 동작 기준
- `Broadcast`는 송신자 자신을 제외하고 fan-out 한다.
- 더미의 기본 RTT 기준은 `ChattingRq -> ChattingRp`다.
- room 전환은 `AckFirst` 정책으로 정리되어, `RoomChangeRp`가 먼저 queue된 뒤 새 room active member로 승격되는 흐름을 기준으로 본다.

## 4. 더미 클라이언트 기준
- room selection mode는 `Random`, `RoundRobin`, `Hotspot`를 지원한다.
- `EventPollMaxCount`는 config에서 조절 가능하며 `0`이면 제한 없이 drain한다.
- 재접속, room 변경, payload 크기, RTT CSV 출력은 config와 bench manifest에서 제어한다.

## 5. 현재 미포함 범위
- `Node.js LoginServer`, `Redis` 기반 chat ticket 검증, `MySQL AccountDB`는 아직 미구현이다.
- `C# WinForms ChattingClient`는 1차 프로토타입을 추가했다.
- 현재 `WinForms` 로그인/회원가입은 mock 성공 기준이며, 실제 계정 검증과 회원가입 저장은 아직 붙지 않았다.
- 현재 benchmark와 성능 비교 중심 축은 여전히 `ChattingDummyClient`다.

## 6. 관련 핵심 문서
- [002_chattingserver-packet-and-flow-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\009_chatting_server\002_chattingserver-packet-and-flow-plan.md)
- [003_chattingdummy-load-test-client-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\009_chatting_server\003_chattingdummy-load-test-client-plan.md)
- [001_nodejs-loginserver-and-winforms-chatting-prototype-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\012_login-platform\001_nodejs-loginserver-and-winforms-chatting-prototype-plan.md)
- [021_chattingserver-128b-10m-backend-benchmark-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\021_chattingserver-128b-10m-backend-benchmark-review.md)
- [005_benchmark-current.md](D:\Project\ServerPortfolio\RefactoringServer\docs\current\005_benchmark-current.md)
