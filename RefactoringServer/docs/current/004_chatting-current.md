# 004 Chatting Current

Status: Active  
Canonical: Yes  
Last Updated: 2026-04-09  
Scope: ChattingServer, Chatting clients, login flow, C# client integration

## 1. 현재 구성
- 서버: [ChattingServer](D:\Project\ServerPortfolio\RefactoringServer\Chatting\ChattingServer)
- 벤치 및 부하 테스트 클라이언트: [ChattingDummyClient](D:\Project\ServerPortfolio\RefactoringServer\Chatting\ChattingDummyClient)
- 사용자용 프로토타입 클라이언트: [ChattingClientWinForms](D:\Project\ServerPortfolio\RefactoringServer\Chatting\ChattingClientWinForms)
- C++ 공용 네트워크 계층: [ClientNetworkLib](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ClientNetworkLib)
- C# 공용 네트워크 계층: [ClientNetworkLib.CSharp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ClientNetworkLib.CSharp)
- C# 패킷 계약 계층: [PacketRuntime.CSharp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\PacketRuntime.CSharp)
- 생성된 C# 패킷 어셈블리: [GeneratedPackets.CSharp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\GeneratedPackets.CSharp)

## 2. 현재 패킷 범위
- `LoginRq / LoginRp`
- `LoginAuthRq / LoginAuthRp`
- `RoomListRq / RoomListRp`
- `RoomChangeRq / RoomChangeRp`
- `ChattingRq / ChattingRp`
- `Broadcast`

## 3. 현재 서버 동작 기준
- `ChattingServer`는 room fan-out과 backend 비교를 위한 샘플 채팅 서버다.
- RTT 기준은 기본적으로 `ChattingRq -> ChattingRp`다.
- room 전환은 `AckFirst` 정책으로 정리되어 있다.
  - `RoomChangeRp`를 먼저 송신 큐에 넣고
  - 그 다음 새 room active member로 승격한다.
- 로그인 경로는 두 가지를 유지한다.
  - 레거시 `LoginRq`
  - `LoginServer HTTP 로그인 -> Redis ticket -> LoginAuthRq`

## 4. 현재 클라이언트 구분
- `ChattingDummyClient`
  - benchmark, 부하 테스트, 재현 실험 전용이다.
  - `Random`, `RoundRobin`, `Hotspot` room selection mode를 지원한다.
  - `EventPollMaxCount`로 이벤트 drain 정책을 조절한다.
- `ChattingClientWinForms`
  - 사용자 조작용 C# 프로토타입 클라이언트다.
  - `LoginServer` 인증 후 `LoginAuthRq`로 채팅 서버에 들어간다.
  - 현재는 UI와 상태 표시를 담당하는 첫 C# 소비자다.

## 5. C# 경로 현재 상태
- `PacketGenerator`는 C# 출력 스캐폴드를 지원한다.
- 생성 결과는 [Generated\CSharp\Packets](D:\Project\ServerPortfolio\RefactoringServer\Generated\CSharp\Packets)에 나온다.
- `ClientNetworkLib.CSharp`는 공용 TCP transport, framing, checksum, packet registry decode를 담당한다.
- `ChattingClientWinForms`는 더 이상 임시 전용 packet codec에 묶여 있지 않고, 공용 C# 네트워크 계층을 사용한다.
- `Generated/CSharp/Packets`와 `ClientNetworkLib.CSharp`는 직접 의존하지 않는다.
  - 중간에 [PacketRuntime.CSharp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\PacketRuntime.CSharp) 계약층을 둔다.

## 6. 런타임 검증 상태
- WinForms 클라이언트에는 `--smoke` 모드가 추가되어 있다.
- 이 모드는 UI 없이 실제 서버 흐름을 검증한다.
  - 회원가입 또는 기존 계정 사용
  - `LoginServer` HTTP 로그인
  - `ChattingServer` TCP 연결
  - `LoginAuthRq`
  - `RoomListRq`
  - `RoomChangeRq`
  - `ChattingRq`
- 2026-04-09 기준 실제 smoke 결과는 성공이다.
  - `RegisterAsync succeeded`
  - `LoginAsync succeeded`
  - `LoginResultReceived: success=True`
  - `RoomListReceived: roomCount=50`
  - `RoomChangeResultReceived: success=True`
  - `ChattingResultReceived: success=True`

## 7. 현재 미완료 범위
- WinForms UI의 사용성 개선
- C# reconnect 및 오류 복구 정책 정리
- `Broadcast` 수신과 다중 세션 상호작용 검증 보강
- Unity 재사용을 위한 public API 다듬기
- 필요 시 `Generated/CSharp/Config` 확장 검토

## 8. 다음 우선순위
1. `ClientNetworkLib.CSharp` 오류 경로와 reconnect 정책 보강
2. WinForms 수동 시나리오 검증
3. Unity 재사용을 고려한 API 표면 정리
4. 필요 시 C# config 생성 지원 검토

## 9. 먼저 볼 문서
- [001_csharp-clientnetworklib-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\012_csharp_support\001_csharp-clientnetworklib-plan.md)
- [002_packetgenerator-csharp-output-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\012_csharp_support\002_packetgenerator-csharp-output-plan.md)
- [001_nodejs-loginserver-and-winforms-chatting-prototype-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\012_login-platform\001_nodejs-loginserver-and-winforms-chatting-prototype-plan.md)
- [026_chattingserver-windowsserver-1h-random-4mode-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\026_chattingserver-windowsserver-1h-random-4mode-review.md)
- [028_chatting-winforms-csharp-clientnetworklib-smoke-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\028_chatting-winforms-csharp-clientnetworklib-smoke-review.md)
- [005_benchmark-current.md](D:\Project\ServerPortfolio\RefactoringServer\docs\current\005_benchmark-current.md)
