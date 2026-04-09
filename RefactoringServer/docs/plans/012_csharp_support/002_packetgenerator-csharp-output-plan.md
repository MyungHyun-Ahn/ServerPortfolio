# PacketGenerator C# 출력 계획

## 1. 목적
- `PacketGenerator`가 기존 C++ 출력만이 아니라 C# packet / codec 출력도 지원하도록 확장한다.
- 1차 소비자는 `ChattingClient.WinForms`이지만, 생성 결과는 추후 `UnityClient`에서도 그대로 재사용 가능해야 한다.
- 서버, 더미 클라이언트, C# 클라이언트가 동일한 `YAML` packet schema를 공유하도록 유지한다.

## 2. 현재 상태
- `Tools/PacketGenerator`는 C#으로 작성된 오프라인 생성기다.
- schema type 검증 단계에서 C# 타입 매핑은 이미 일부 고려되고 있다.
- 하지만 실제 생성 결과는 아직 C++ 전용이다.
  - `<Content>Packets.h`
  - `<Content>PacketHandler.h`
  - `PacketRouter.h`
- `Generated/CSharp/**` 출력 경로와 C# serializer / deserializer / decoder 생성은 아직 없다.

## 3. 왜 먼저 PacketGenerator를 확장해야 하는가
- `ClientNetworkLib.CSharp`를 새로 만들더라도 packet contract를 수동으로 이중 관리하면 유지보수 비용이 급격히 커진다.
- `ChattingClient.WinForms`와 `UnityClient`가 같은 네트워크 계층을 쓰려면, UI 프레임워크와 무관한 generated C# packet/codec가 먼저 필요하다.
- native `ClientNetworkLib` bridge 방식보다, 공통 schema 기반 C# packet/codegen 경로를 먼저 여는 쪽이 더 안전하다.

## 4. 1차 목표
1. `YAML -> Generated/CSharp` 출력 경로를 추가한다.
2. message별 C# packet type과 serialize / deserialize 코드를 생성한다.
3. opcode 상수와 packet decode registry를 생성한다.
4. `ChattingServer`와 C# 클라이언트 간 byte parity 검증 경로를 만든다.

## 5. 1차 비목표
- WinForms 전용 UI binding 코드 생성
- Unity 전용 `MonoBehaviour` / `ScriptableObject` 코드 생성
- server-side C# handler / router 생성
- `P/Invoke`, `C++/CLI` bridge 생성
- zero-copy / `Span<T>` 중심 최적화

## 6. 출력 범위
### 6.1 생성 대상
- content별 packet type
- opcode / content id 상수
- packet별 `Serialize` / `Deserialize`
- packet decode registry 또는 factory
- 공용 packet header 상수

### 6.2 1차 출력 예시
- `Generated/CSharp/Packets/Chat/ChatPackets.g.cs`
- `Generated/CSharp/Packets/Chat/ChatPacketCodec.g.cs`
- `Generated/CSharp/Packets/PacketRegistry.g.cs`
- `Generated/CSharp/Packets/PacketOpcodes.g.cs`

### 6.3 1차 제외 대상
- C# content handler skeleton
- C# packet router
- benchmark / diagnostics 전용 packet helper

## 7. 타입 매핑 정책
### 7.1 scalar
- `bool -> bool`
- `int8 -> sbyte`
- `uint8 -> byte`
- `int16 -> short`
- `uint16 -> ushort`
- `int32 -> int`
- `uint32 -> uint`
- `int64 -> long`
- `uint64 -> ulong`
- `float -> float`
- `double -> double`

### 7.2 special
- `string -> string`
- `bytes -> byte[]`
- `string_view -> string`
- `bytes_view -> byte[]`

### 7.3 container
- `vector<T> -> List<T>`
- `array<T, N> -> T[]`
- `map<K, V> -> Dictionary<K, V>`
- `unordered_map<K, V> -> Dictionary<K, V>`

### 7.4 정책 메모
- C++의 borrowed view 의미론을 C# generated code에 그대로 옮기지 않는다.
- 1차 C# 출력은 Unity 호환성과 단순성을 위해 소유형 중심으로 간다.
- 기존 정책대로 container depth는 1단계까지만 공식 지원한다.

## 8. 생성 코드 형태
### 8.1 packet type
- packet은 `sealed partial class` 또는 `sealed record class` 중 하나로 통일한다.
- 1차 추천은 `sealed partial class`다.

이유:
- 생성 코드와 수기 확장 코드를 partial로 나누기 쉽다.
- Unity/WinForms에서 모두 익숙한 형태다.
- 향후 custom helper를 수기 파일로 붙이기 쉽다.

### 8.2 codec API
- 각 packet은 `Serialize(PacketWriter writer)` / `bool Deserialize(PacketReader reader)` 형태를 가진다.
- C# 네트워크 계층이 packet object와 raw payload 사이를 직접 연결할 수 있어야 한다.

### 8.3 decode registry
- `opcode -> decoder delegate` registry를 생성한다.
- `ClientNetworkLib.CSharp`는 이 registry를 받아 공통 decode path를 구성한다.

## 9. 출력 경로 구조
- `Generated/CSharp/Packets/<Content>/`
- `Generated/CSharp/Shared/`

추천 파일 분리:
- content별 packet 정의
- content별 codec helper
- 전역 opcode/registry

이 구조를 택하는 이유:
- C++ generated 구조와 대응 관계를 유지하기 쉽다.
- 특정 content만 diff 확인하기 쉽다.
- WinForms / Unity 공용 프로젝트에서 참조하기 쉽다.

## 10. Generator 구조 변경 방향
### 10.1 유지할 것
- 기존 `CppPacketGenerator`
- 기존 schema parser / validator
- 기존 `Program.cs` 엔트리포인트

### 10.2 추가할 것
- `CSharpPacketGenerator`
- C# 출력용 template/helper
- target 선택 인자

### 10.3 CLI 추천안
- `--targets cpp`
- `--targets csharp`
- `--targets cpp,csharp`

1차 정책:
- 기본값은 기존과 같은 `cpp` 유지
- C# 경로가 실제 소비되기 시작하면 `scripts/generate`에서 `cpp,csharp`를 기본으로 넘기도록 올린다.

## 11. C# 런타임 호환 정책
- generated code는 `netstandard2.0` 기준으로 컴파일 가능해야 한다.
- `System.Windows.Forms`, `UnityEngine` 참조를 포함하지 않는다.
- `Span<T>`, `Memory<T>`, source generator 의존은 1차에서 넣지 않는다.

이유:
- WinForms와 Unity 양쪽 재사용성을 우선 확보해야 한다.
- packet/codegen 계층은 최대한 낮은 공통 분모를 유지하는 편이 안전하다.

## 12. 검증 계획
### 12.1 generator 검증
- schema 파싱 성공/실패 케이스
- unsupported type 실패
- duplicated opcode 실패
- `rq/rp` pair 규칙 유지

### 12.2 parity 검증
- C++ serialize -> C# deserialize
- C# serialize -> C++ deserialize
- `Chat.yaml`, `Echo.yaml` 기준 round-trip 검증

### 12.3 compile 검증
- generated C# 파일만 모아도 컴파일 성공
- `ClientNetworkLib.CSharp`에서 실제 참조 가능

## 13. 구현 순서
1. `Program.cs`에 target 선택 구조를 추가한다.
2. `Generated/CSharp` 출력 경로와 파일 naming rule을 확정한다.
3. `CSharpPacketGenerator`에서 packet type 생성을 구현한다.
4. serialize / deserialize 생성 코드를 붙인다.
5. opcode / registry 생성을 붙인다.
6. `Echo`, `Chat` schema 기준 parity test를 추가한다.
7. 이후 `ClientNetworkLib.CSharp`가 이 생성 결과를 실제로 사용하도록 연결한다.

## 14. 1차 성공 기준
- `PacketGenerator`가 `cpp,csharp` 동시 출력을 지원한다.
- `Generated/CSharp`가 `ChattingServer` packet schema를 기준으로 정상 생성된다.
- generated C# packet이 C++ wire format과 호환된다.
- `ClientNetworkLib.CSharp`가 수동 DTO 없이 generated packet만으로 decode 경로를 구성할 수 있다.

## 15. 최종 추천
- 먼저 `PacketGenerator` C# 출력을 열고,
- 그 위에 `ClientNetworkLib.CSharp`를 올리는 순서가 맞다.
- `ChattingClient.WinForms`는 그 첫 소비자이고,
- 설계 기준은 처음부터 `UnityClient` 재사용 가능성까지 포함해야 한다.

## 16. 의존성 분리 원칙
- generated packet 코드와 `ClientNetworkLib.CSharp`는 서로 직접 의존하지 않는다.
- 두 계층 사이에는 `PacketRuntime.CSharp` 계약층을 둔다.

권장 의존 방향:
- `PacketRuntime.CSharp`
  - 공용 writer / reader
  - packet interface
  - registry interface
- `Generated/CSharp/Packets`
  - `PacketRuntime.CSharp`만 참조
- `ClientNetworkLib.CSharp`
  - `PacketRuntime.CSharp`만 참조
- 실제 소비자
  - `ChattingClient.WinForms`
  - `UnityClient`
  - 여기서 generated packet과 network lib를 함께 조립

즉, 본 계획서의 “생성 결과를 실제로 사용하도록 연결한다”는 의미는
- generated packet 코드가 network lib를 직접 참조한다는 뜻이 아니라
- 두 계층이 공용 계약층 위에서 런타임에 조립된다는 뜻으로 해석한다.
