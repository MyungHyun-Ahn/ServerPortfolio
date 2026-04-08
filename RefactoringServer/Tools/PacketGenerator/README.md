# PacketGenerator

간단한 YAML 패킷 스키마를 읽어서 C++ 패킷/핸들러/라우터 헤더를 생성하는 오프라인 도구입니다.

## 역할
- `RefactoringServer/Packet/**/*.yaml` 스키마를 읽는다.
- `RefactoringServer/Generated/Packets/**` 아래에 generated C++ 헤더를 만든다.
- 콘텐츠별 packet/handler와 전역 `PacketRouter.h`를 생성한다.

## 기본 실행

프로젝트 루트에서:

```powershell
RefactoringServer\scripts\generate\Generate-Packets.cmd
```

직접 실행할 때:

```powershell
dotnet run --project RefactoringServer\Tools\PacketGenerator\PacketGenerator.csproj
```

## 인자
- `--schema-root <path>`
  - 기본값: `RefactoringServer/Packet`
- `--output-root <path>`
  - 기본값: `RefactoringServer/Generated/Packets`

예:

```powershell
dotnet run --project RefactoringServer\Tools\PacketGenerator\PacketGenerator.csproj -- --schema-root D:\Project\ServerPortfolio\RefactoringServer\Packet --output-root D:\Project\ServerPortfolio\RefactoringServer\Generated\Packets
```

## 스키마 규칙
- 파일 1개가 콘텐츠 1개다.
- 예:
  - `Packet/Echo/Echo.yaml`
  - `Packet/Login/Login.yaml`
  - `Packet/Chat/Chat.yaml`
- 한 파일 안에 여러 `messages`를 둘 수 있다.
- `rq`와 `rp`는 반드시 쌍으로 존재해야 한다.
- `noti`는 선택 사항이다.
- `rq`, `rp`, `noti`는 각각 다른 `opcode`를 가져야 한다.
- 전체 스키마 집합에서 `content` 이름과 `opcode`는 중복되면 안 된다.

## 지원 타입
- scalar
  - `bool`
  - `int8`, `int16`, `int32`, `int64`
  - `uint8`, `uint16`, `uint32`, `uint64`
  - `float`
  - `double`
- special
  - `string`
  - `bytes`
- container
  - `vector<T>`
  - `array<T, N>`
  - `map<K, V>`
  - `unordered_map<K, V>`

## 컨테이너 정책
- 공식 지원은 `컨테이너 1단계`까지만이다.
- 허용 예:
  - `vector<string>`
  - `map<string, uint32>`
  - `unordered_map<string, string>`
- 금지 예:
  - `vector<vector<int32>>`
  - `map<string, vector<uint32>>`
  - `unordered_map<string, map<string, int32>>`

복잡한 nested container가 필요하면 생성기 기본 규칙을 확장하기보다, 생성된 패킷에서 `Serialize` / `Deserialize`를 수동 override하는 쪽을 우선 권장합니다.

## 생성 결과
- 콘텐츠별:
  - `<Content>Packets.h`
  - `<Content>PacketHandler.h`
- 전역:
  - `PacketRouter.h`

예:
- `Generated/Packets/Echo/EchoPackets.h`
- `Generated/Packets/Echo/EchoPacketHandler.h`
- `Generated/Packets/PacketRouter.h`

## 생성 코드 구조
- packet class는 `IContentPacket`을 따른다.
- 기본적으로 아래를 생성한다.
  - `GetOpcode()`
  - `virtual Serialize(FPacketWriter&) const`
  - `virtual Deserialize(FPacketReader&)`
- 콘텐츠별 handler base / dispatcher도 같이 생성한다.
- 전역 router가 `opcode`로 어떤 콘텐츠 처리기에 넘길지 선택한다.

## 실패 조건
- `rq`만 있고 `rp`가 없을 때
- `rp`만 있고 `rq`가 없을 때
- 지원하지 않는 타입이 들어왔을 때
- 중복 `opcode`가 있을 때
- 중복 `content` 이름이 있을 때
- YAML 파싱이 실패할 때

## 권장 작업 흐름
1. `Packet/**/*.yaml` 수정
2. `Generate-Packets.cmd` 실행
3. `Generated/Packets/**` 변경 확인
4. C++ 솔루션 빌드

## 참고 문서
- [Content Header And Packet CodeGen Plan](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\006_packet-schema-tooling\001_content-header-and-packet-codegen.md)
- [Packet Container Support Policy](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\networklib\packet-container-support-policy.md)
