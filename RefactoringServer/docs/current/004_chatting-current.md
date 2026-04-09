# 004 Chatting Current

Status: Active  
Canonical: Yes  
Last Updated: 2026-04-08  
Scope: ChattingServer, ClientNetworkLib, ChattingDummy current state

## 1. 현재 프로젝트 구성
- 서버: [ChattingServer](D:\Project\ServerPortfolio\RefactoringServer\Chatting\ChattingServer)
- 더미 부하 클라이언트: [ChattingDummyClient](D:\Project\ServerPortfolio\RefactoringServer\Chatting\ChattingDummyClient)
- 공용 클라이언트 네트워크 라이브러리: [ClientNetworkLib](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ClientNetworkLib)

## 2. 현재 packet surface
- `LoginRq / LoginRp`
- `RoomListRq / RoomListRp`
- `RoomChangeRq / RoomChangeRp`
- `ChattingRq / ChattingRp`
- `Broadcast`

## 3. 현재 동작 기준
- `Broadcast`는 송신자 본인을 제외하고 fan-out 한다.
- 더미의 기본 RTT 기준은 `ChattingRq -> ChattingRp`다.
- room 전환은 `AckFirst` 정책으로 정리되어, `RoomChangeRp`가 먼저 queue에 들어간 뒤 room active member로 승격된다.

## 4. 현재 더미 클라이언트 기준
- room selection mode는 `Random`, `RoundRobin`, `Hotspot`을 지원한다.
- `EventPollMaxCount`는 config에서 조절 가능하며 `0`이면 제한 없이 drain 한다.
- 재접속, room 변경, payload 크기, RTT CSV 출력은 config와 benchmark manifest에서 제어한다.

## 5. 현재 미포함 범위
- C# WinForms `ChattingClient`는 아직 구현하지 않았다.
- 현재는 benchmark와 성능 비교를 위한 `ChattingDummyClient`가 우선이다.

## 6. 다음 큰 축
- 다음 후속은 `C# WinForms ChattingClient` 추가다.
- 현재 추천 방향은:
  - `PacketGenerator`에 C# 출력 지원 추가
  - `ClientNetworkLib.CSharp` 신규 구현
  - `ChattingClient.WinForms`는 C# packet + C# network lib 사용
  - 이후 `UnityClient`도 같은 C# 공용 라이브러리를 재사용
- native `ClientNetworkLib`를 C#에서 직접 재사용하는 bridge 방식은 2차 대안으로 남겨둔다.

## 7. 관련 최신 문서
- [002_chattingserver-packet-and-flow-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\009_chatting_server\002_chattingserver-packet-and-flow-plan.md)
- [003_chattingdummy-load-test-client-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\009_chatting_server\003_chattingdummy-load-test-client-plan.md)
- [001_csharp-clientnetworklib-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\012_csharp_support\001_csharp-clientnetworklib-plan.md)
- [002_packetgenerator-csharp-output-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\012_csharp_support\002_packetgenerator-csharp-output-plan.md)
- [021_chattingserver-128b-10m-backend-benchmark-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\021_chattingserver-128b-10m-backend-benchmark-review.md)
- [005_benchmark-current.md](D:\Project\ServerPortfolio\RefactoringServer\docs\current\005_benchmark-current.md)
