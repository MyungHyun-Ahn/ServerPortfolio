# 004 Chatting Current

Status: Active
Canonical: Yes
Last Updated: 2026-04-09
Scope: ChattingServer, Chatting clients, login flow, benchmark current state

## 1. 현재 프로젝트 구성
- 서버: [ChattingServer](D:\Project\ServerPortfolio\RefactoringServer\Chatting\ChattingServer)
- 부하/벤치 클라이언트: [ChattingDummyClient](D:\Project\ServerPortfolio\RefactoringServer\Chatting\ChattingDummyClient)
- 사용자용 프로토타입 클라이언트: [ChattingClientWinForms](D:\Project\ServerPortfolio\RefactoringServer\Chatting\ChattingClientWinForms)
- 공용 네이티브 클라이언트 네트워크 라이브러리: [ClientNetworkLib](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ClientNetworkLib)

## 2. 현재 packet surface
- `LoginRq / LoginRp`
- `LoginAuthRq / LoginAuthRp`
- `RoomListRq / RoomListRp`
- `RoomChangeRq / RoomChangeRp`
- `ChattingRq / ChattingRp`
- `Broadcast`

## 3. 현재 서버 동작 기준
- room fan-out과 backend 비교를 위한 샘플 채팅 서버로 유지한다.
- `Broadcast`는 송신자를 제외하고 fan-out한다.
- 기본 RTT 기준은 `ChattingRq -> ChattingRp`이다.
- room 전환은 `AckFirst` 정책으로 정리되어 `RoomChangeRp`가 먼저 queue에 들어가고 그 뒤에 새 room active member로 승격된다.
- 로그인 경로는 기존 `LoginRq`와 외부 인증용 `LoginAuthRq`가 병행 가능하다.

## 4. 현재 클라이언트 구분
- `ChattingDummyClient`
  - benchmark와 장시간 부하 테스트 전용이다.
  - `Random`, `RoundRobin`, `Hotspot` room selection mode를 지원한다.
  - `EventPollMaxCount`로 이벤트 drain 정책을 조절한다.
- `ChattingClientWinForms`
  - 사용자 시연용 C# 프로토타입 클라이언트다.
  - `LoginServer HTTP 로그인/회원가입 -> LoginAuthRq -> 채팅 서버 접속` 흐름을 사용한다.
  - 로그인 실패 코드는 팝업 메시지로 매핑되어 있다.

## 5. 현재 로그인 플랫폼 연동 상태
- `Node.js LoginServer`가 `MySQL`, `Redis`, `Argon2id`, Swagger까지 포함한 1차 범위로 구현되어 있다.
- 로그인 성공 시 Redis에 1회성 chat ticket을 기록하고, `ChattingServer`는 `LoginAuthRq`에서 이를 consume한다.
- `userId -> sessionId` 기준 중복 로그인 강제 종료 흐름까지 반영되어 있다.
- 로컬/벤치 호환을 위해 기존 insecure `LoginRq` 경로도 유지한다.

## 6. 현재 미완료 범위
- `ChattingClientWinForms`는 프로토타입 UI 단계이며 Unity 공용화는 아직 아니다.
- `PacketGenerator` C# 출력과 `ClientNetworkLib.CSharp`는 설계/초기 작업 단계다.
- 장기적으로는 WinForms와 Unity가 공용 C# packet/runtime 계층을 함께 쓰는 구조로 수렴할 예정이다.

## 7. 다음 핵심 작업
- `PacketGenerator` C# 출력 범위 확정과 parity 검증
- `ClientNetworkLib.CSharp` 공용 계층 설계/구현
- WinForms와 향후 UnityClient가 재사용 가능한 packet/runtime 경계 정리
- `LoginAuthRq` 경로와 benchmark 경로를 함께 유지하는 운영 기준 정리

## 8. 관련 최신 문서
- [006_loginserver-current.md](D:\Project\ServerPortfolio\RefactoringServer\docs\current\006_loginserver-current.md)
- [002_chattingserver-packet-and-flow-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\009_chatting_server\002_chattingserver-packet-and-flow-plan.md)
- [003_chattingdummy-load-test-client-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\009_chatting_server\003_chattingdummy-load-test-client-plan.md)
- [001_nodejs-loginserver-and-winforms-chatting-prototype-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\012_login-platform\001_nodejs-loginserver-and-winforms-chatting-prototype-plan.md)
- [001_csharp-clientnetworklib-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\012_csharp_support\001_csharp-clientnetworklib-plan.md)
- [002_packetgenerator-csharp-output-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\012_csharp_support\002_packetgenerator-csharp-output-plan.md)
- [021_chattingserver-128b-10m-backend-benchmark-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\021_chattingserver-128b-10m-backend-benchmark-review.md)
- [026_chattingserver-windowsserver-1h-random-4mode-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\026_chattingserver-windowsserver-1h-random-4mode-review.md)
- [005_benchmark-current.md](D:\Project\ServerPortfolio\RefactoringServer\docs\current\005_benchmark-current.md)
