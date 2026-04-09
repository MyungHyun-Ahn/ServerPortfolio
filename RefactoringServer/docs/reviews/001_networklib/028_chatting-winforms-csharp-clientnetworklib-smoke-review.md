# 028 Chatting WinForms C# ClientNetworkLib Smoke Review

Status: Active  
Canonical: Yes  
Last Updated: 2026-04-09  
Scope: ChattingClientWinForms / ClientNetworkLib.CSharp / LoginServer integration

## 1. 목적
- 급하게 붙여두었던 WinForms 전용 네트워크 경로를 공용 C# 계층으로 교체한 뒤, 실제 서버 흐름에서 정상 동작하는지 확인한다.
- 이번 검증의 핵심 질문은 다음 두 가지다.
  - `ClientNetworkLib.CSharp + GeneratedPackets.CSharp` 조합이 실제 채팅 서버와 호환되는가
  - `LoginServer HTTP 로그인 -> LoginAuthRq -> RoomList -> RoomChange -> Chatting` 경로가 UI 없이도 자동 검증 가능한가

관련 구현:
- [ContentTcpClient.cs](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ClientNetworkLib.CSharp\ContentTcpClient.cs)
- [GeneratedPacketRegistry.g.cs](D:\Project\ServerPortfolio\RefactoringServer\Generated\CSharp\Packets\GeneratedPacketRegistry.g.cs)
- [ChattingTcpClient.cs](D:\Project\ServerPortfolio\RefactoringServer\Chatting\ChattingClientWinForms\Networking\ChattingTcpClient.cs)
- [Program.cs](D:\Project\ServerPortfolio\RefactoringServer\Chatting\ChattingClientWinForms\Program.cs)
- [ChattingClientSmokeRunner.cs](D:\Project\ServerPortfolio\RefactoringServer\Chatting\ChattingClientWinForms\ChattingClientSmokeRunner.cs)

## 2. 테스트 환경
- 로그인 서버: `http://127.0.0.1:18080`
- 채팅 서버: `127.0.0.1:19100`
- 로그인 티켓 Redis: `127.0.0.1:6380`
- packet key: `55`
- 채팅 서버 실행 방식: `--headless`

관련 설정:
- [ChattingServer.yaml](D:\Project\ServerPortfolio\RefactoringServer\Out\ChattingServer\Config\Server\ChattingServer.yaml)

## 3. 실행 경로
### 3-1. 채팅 서버
```powershell
cd D:\Project\ServerPortfolio\RefactoringServer\Out\ChattingServer
.\ChattingServer.exe --headless
```

### 3-2. WinForms 스모크

```powershell
cd D:\Project\ServerPortfolio\RefactoringServer\Out\ChattingClientWinForms\Debug\net9.0-windows
.\ChattingClientWinForms.exe --smoke --host 127.0.0.1 --port 19100 --login-server http://127.0.0.1:18080 --timeout-seconds 45
```

## 4. 스모크 모드가 검증하는 단계
1. 필요 시 테스트 계정 회원가입
2. `LoginServer` HTTP 로그인
3. `ChattingServer` TCP 연결
4. `LoginAuthRq`
5. `RoomListRq`
6. joinable room 선택 후 `RoomChangeRq`
7. `ChattingRq`
8. `ChattingRp` 성공 확인 후 종료

## 5. 결과 요약
- 종료 코드: `0`
- 회원가입: 성공 또는 기존 계정이면 skip
- HTTP 로그인: 성공
- `LoginAuthRq`: 성공
- `RoomListRq`: 성공, `roomCount=50`
- `RoomChangeRq`: 성공, `currentRoomId=77`
- `ChattingRq`: 성공

대표 로그:
- `RegisterAsync succeeded`
- `LoginAsync succeeded`
- `LoginResultReceived: success=True`
- `RoomListReceived: roomCount=50`
- `RoomChangeResultReceived: success=True`
- `ChattingResultReceived: success=True`
- `Smoke completed successfully.`

## 6. 해석
### 6-1. 컴파일 통합을 넘어서 런타임 연동까지 확인됐다
- 이번 결과는 단순히 `ClientNetworkLib.CSharp`가 빌드된다는 수준을 넘는다.
- 실제 로그인 서버, 실제 채팅 서버, 실제 생성 패킷을 붙인 상태에서 end-to-end 경로가 성공했다.

### 6-2. WinForms 전용 임시 코덱 의존성이 줄었다
- 기존에는 WinForms 프로젝트 안에 임시 packet codec 성격의 코드가 들어가 있었고, 공용화에 불리했다.
- 지금은 transport와 packet decode가 공용 C# 계층으로 올라왔다.
- WinForms는 UI와 사용자 흐름을 담당하는 첫 소비자 역할로 정리되었다.

### 6-3. Unity 재사용 가능성도 한 단계 올라갔다
- `Generated/CSharp/Packets`와 `ClientNetworkLib.CSharp`는 직접 묶이지 않고, [PacketRuntime.CSharp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\PacketRuntime.CSharp)를 사이에 둔다.
- 이 구조는 추후 `UnityClient`에서도 같은 packet/runtime 조합을 재사용하기에 유리하다.

## 7. 남은 작업
- WinForms 실제 UI 상호작용 수동 검증
- disconnect, reconnect, protocol error 경로 보강
- 다중 클라이언트 환경에서 `Broadcast` 수신 검증
- 필요 시 smoke 시나리오를 benchmark나 CI 전용 경로로 확장

## 8. 결론
- `ClientNetworkLib.CSharp` 1차 도입은 성공적이다.
- `ChattingClientWinForms`는 이제 공용 C# 네트워크 계층을 사용하는 실제 소비자이며, 최소 핵심 채팅 플로우가 런타임에서 검증되었다.
- 다음 단계는 새 계층을 안정화하고, UI 수동 검증과 Unity 재사용을 위한 API 정리를 진행하는 것이다.
