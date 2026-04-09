# C# ClientNetworkLib 계획

## 1. 목적
- C# 기반 공용 클라이언트 네트워크 라이브러리 `ClientNetworkLib.CSharp`를 설계한다.
- 1차 소비자는 `ChattingClient.WinForms`지만, 추후 `UnityClient`에서도 재사용 가능해야 한다.
- 서버와 더미 클라이언트가 이미 사용하는 packet contract와 wire format을 C#에서도 동일하게 해석할 수 있어야 한다.

## 2. 왜 별도 축으로 분리하는가
- `ChattingDummyClient`는 benchmark / 부하 테스트용이다.
- `ChattingClient.WinForms`는 사람이 직접 조작하는 UI 클라이언트다.
- `UnityClient`는 또 다른 런타임 제약과 빌드 환경을 가진다.
- 따라서 지금 필요한 것은 “WinForms 전용 코드”가 아니라, `WinForms`와 `Unity`가 같이 사용할 수 있는 공용 C# transport / codec 계층이다.

## 3. 최상위 목표
1. `PacketGenerator`가 C# packet / codec 출력을 지원한다.
2. `ClientNetworkLib.CSharp`는 UI 프레임워크 비의존적으로 설계한다.
3. `ChattingClient.WinForms`는 이 라이브러리를 첫 소비자로 사용한다.
4. 이후 `UnityClient`는 같은 packet/codec와 같은 transport 인터페이스를 재사용한다.

## 4. 추천 방향
### 추천안
- 기존 native `ClientNetworkLib`를 C#에서 직접 재사용하지 않는다.
- `ClientNetworkLib.CSharp`를 새로 구현한다.
- `PacketGenerator`에 C# 출력 지원을 추가한다.

### 추천 이유
- `P/Invoke` / `C++/CLI` bridge는 UI 클라이언트에서 lifetime, callback, marshalling, 배포 복잡도를 크게 올린다.
- WinForms와 Unity는 둘 다 C# 생태계 위에 있으므로, C# 공용 라이브러리가 있으면 프레임워크별 UI만 분리하면 된다.
- 벤치마크용 C++ 경로와 데모/실사용용 C# 경로의 역할이 명확해진다.

## 5. 핵심 원칙
### 5.1 UI 프레임워크 비의존
- `ClientNetworkLib.CSharp`는 아래 의존성을 가지지 않는다.
  - `System.Windows.Forms`
  - `UnityEngine`
  - WPF / MAUI / Avalonia 전용 API

### 5.2 프로토콜 공통
- 서버, `ChattingDummyClient`, C# 클라이언트는 같은 packet schema를 공유한다.
- C++와 C#의 serializer / deserializer 결과는 byte parity를 검증한다.

### 5.3 안전성 우선
- benchmark용 극한 최적화보다, 예측 가능한 lifetime / reconnect / disconnect 처리를 우선한다.
- UI thread와 network I/O thread를 명확히 분리한다.

### 5.4 Unity 재사용성 우선
- 초기에 WinForms용으로 시작하더라도, public API는 Unity에서 그대로 쓸 수 있게 설계한다.
- 즉 “WinForms에 맞는 라이브러리”가 아니라 “WinForms도 사용하는 라이브러리”로 간다.

## 6. 대상 프레임워크 권장안
### 6.1 Core library
- 1차 권장: `netstandard2.0`

이유:
- Unity 호환 폭이 가장 넓다.
- WinForms 소비자도 참조 가능하다.
- packet / transport / event queue 수준에서는 충분하다.

### 6.2 WinForms client
- `net8.0-windows` 또는 팀 표준 .NET Windows target

### 6.3 향후 확장
- 성능 최적화가 필요하면 나중에 `net8.0` 추가 multi-target을 검토한다.
- 하지만 1차는 Unity 호환 폭을 넓게 가져가는 쪽이 더 중요하다.

## 7. 권장 프로젝트 구조
- `Libraries/ClientNetworkLib.CSharp`
- `Generated/CSharp/Packets/...`
- `Generated/CSharp/Shared/...`
- `Chatting/ChattingClient.WinForms`
- 향후 `Unity/UnityClient` 또는 별도 Unity 프로젝트에서 `ClientNetworkLib.CSharp` 참조

## 8. ClientNetworkLib.CSharp 범위
### 포함
- connect / disconnect
- reconnect policy hook
- send queue
- recv buffer / frame parser
- checksum / cipher
- packet codec
- event dispatch
- session state
- protocol error / transport error reporting

### 제외
- WinForms control 갱신
- Unity scene / MonoBehaviour 로직
- 채팅 room UI
- benchmark 통계 로직

## 9. PacketGenerator C# 지원 범위
### 9.1 1차 범위
- C# packet class 생성
- packet id / opcode 상수 생성
- `Serialize` / `Deserialize` 생성
- packet decoder helper 생성
- 공통 packet header 상수 생성

### 9.2 타입 정책
- `int32 -> int`
- `int64 -> long`
- `bool -> bool`
- `string -> string`
- `bytes -> byte[]`
- list/container -> `List<T>`

### 9.3 view 타입 정책
- C++의 `string_view`, `bytes_view`를 C#에서 억지로 대응하지 않는다.
- C# 출력은 우선 소유형으로 단순하게 간다.

### 9.4 출력 위치
- `Generated/CSharp/Packets/<Domain>/...`
- `Generated/CSharp/Shared/...`

## 10. C# NetworkLib 설계 방향
### 10.1 transport 계층
- `Socket`
- `SocketAsyncEventArgs`
- non-blocking / async completion 기반

### 10.2 event 모델
- `Connected`
- `ConnectFailed`
- `Disconnected`
- `PacketReceived`
- `ProtocolError`
- `TransportError`

### 10.3 threading 모델
- network I/O는 background completion 기반
- UI thread 반영은 소비자 쪽에서 처리
- 라이브러리는 UI dispatcher를 모른다

### 10.4 packet 처리 경계
- 라이브러리는 `byte[] -> packet decode -> domain packet object`까지만 책임진다
- WinForms / Unity는 decoded packet을 받아 화면이나 게임 로직에 반영한다

## 11. WinForms와 Unity에서의 사용 방식
### 11.1 WinForms
- `SynchronizationContext.Post(...)`로 UI thread 반영
- connect, room list, chat history, status log를 UI에 표시

### 11.2 Unity
- 라이브러리는 plain C# object만 올려준다
- Unity 쪽은 main thread queue를 통해 scene/UI에 반영한다
- `MonoBehaviour`나 `UnityEngine` 타입은 core library에 들어가지 않는다

## 12. 구현 순서
1. `PacketGenerator`의 C# 출력 설계를 먼저 확정한다.
2. `Chat` schema 기준 C# packet/codegen을 만든다.
3. byte parity 테스트를 추가한다.
4. `ClientNetworkLib.CSharp` 프로젝트를 만든다.
5. connect / recv / send / framing / checksum / cipher를 붙인다.
6. console smoke client로 protocol만 먼저 검증한다.
7. `ChattingClient.WinForms` 프로젝트를 만들어 1차 소비자로 붙인다.
8. 이후 Unity용 소비자 예제를 붙일 수 있게 public API를 정리한다.

## 13. 테스트 계획
### Generator
- C++와 C# packet serialize 결과 byte parity 검증
- C++ server -> C# client decode 검증
- C# client -> C++ server decode 검증

### ClientNetworkLib.CSharp
- connect / disconnect
- partial recv / partial send
- invalid header / invalid checksum
- graceful close / forced close
- reconnect smoke

### Reuse 검증
- WinForms sample consumer 동작
- Unity용 minimal mock consumer 또는 interface sample 검증

## 14. 1차 성공 기준
- `ChattingServer`와 C# client가 로그인, 방 목록 조회, 방 이동, 채팅, broadcast 수신까지 정상 동작한다.
- generated C# packet이 C++ packet과 호환된다.
- `ClientNetworkLib.CSharp` public API가 WinForms 전용 타입 없이 유지된다.
- 문서상 `UnityClient`에서 재사용 가능한 구조가 아니라 실제로도 참조 가능한 구조다.

## 15. 최종 정리
- 지금 만들어야 하는 것은 `WinForms 전용 네트워크 코드`가 아니라 `Unity도 쓸 수 있는 C# 공용 ClientNetworkLib`다.
- 따라서:
  - `PacketGenerator`는 C# 출력을 지원해야 하고
  - `ClientNetworkLib.CSharp`는 새로 구현해야 하며
  - `ChattingClient.WinForms`는 그 첫 소비자가 되는 구조가 가장 적절하다.

## 16. 의존성 경계
- `Generated/CSharp/Packets`와 `ClientNetworkLib.CSharp`는 서로 직접 참조하지 않는 구조가 좋다.
- 중간에 작은 계약층 `PacketRuntime.CSharp`를 둔다.

권장 의존 방향:
- `PacketRuntime.CSharp`
  - `PacketWriter`
  - `PacketReader`
  - `IContentPacket`
  - `IContentPacketRegistry`
- `Generated/CSharp/Packets`
  - `PacketRuntime.CSharp`만 참조
- `ClientNetworkLib.CSharp`
  - `PacketRuntime.CSharp`만 참조
- `ChattingClient.WinForms`, `UnityClient`
  - `Generated/CSharp/Packets`와 `ClientNetworkLib.CSharp`를 함께 참조하고 조립

이 구조를 택하는 이유:
- packet generated code와 transport 구현의 순환 의존을 피할 수 있다.
- Unity 재사용성이 좋아진다.
- packet schema 변경과 network transport 변경의 영향 범위를 분리할 수 있다.
