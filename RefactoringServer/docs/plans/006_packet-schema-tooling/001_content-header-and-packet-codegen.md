# Content Header And Packet CodeGen Plan

## 1. Goal
- `NetworkLib` transport header and content header are separated.
- Packet schema is written per content in `YAML`.
- `PacketGenerator` is a C# offline tool.
- Generated C++ packet classes, handlers, and serialization code are used by server/client code.

## 2. Header Boundary

### 2.1 Transport Header
- Owned by `NetworkLib`.
- Keeps only transport information.
- Current fields:
  - `payloadLength`
  - `randomKey`
  - `checkSum`

### 2.2 Content Header
- Stored at the beginning of payload.
- Owned by content/message layer.
- Current V1 field:
  - `opcode : std::uint16_t`

## 3. Schema Format

### 3.1 File Unit
- One file represents one content group.
- Examples:
  - `Echo.yaml`
  - `Login.yaml`
  - `Chat.yaml`

### 3.2 Message Unit
- One content file can contain multiple messages.
- Each message may define:
  - `rq`
  - `rp`
  - `noti`

### 3.3 Validation Rules
- `rq` and `rp` must exist as a pair.
- If only one of `rq` or `rp` exists, generation fails.
- `noti` is optional.
- `rq`, `rp`, `noti` each use a different `opcode`.
- Duplicate `opcode` in the same schema set is not allowed.

## 4. Type System

### 4.1 Schema Types
- Schema uses language-neutral types, not raw C++ type names.
- Examples:
  - `bool`
  - `int32`
  - `uint64`
  - `float`
  - `double`
  - `string`
  - `bytes`
  - `vector<int32>`
  - `map<string, int32>`

### 4.2 Generator Mapping
- `PacketGenerator` maps schema type to:
  - C++ type
  - C# type
- Example:
  - `int32` -> C++ `std::int32_t`, C# `int`
  - `string` -> C++ `std::string`, C# `string`
  - `bytes` -> C++ `std::vector<std::uint8_t>`, C# `byte[]`

### 4.3 Container Policy
- Supported now:
  - scalar types
  - `string`
  - `vector<T>`
  - `array<T, N>`
  - `map<K, V>`
  - `unordered_map<K, V>`
- Official policy is **one container level only**.
- This means `T`, `K`, and `V` may be scalar or `string`, but may not be another container type.
- Allowed examples:
  - `vector<string>`
  - `map<string, uint32>`
  - `unordered_map<string, string>`
- Rejected examples:
  - `vector<vector<int32>>`
  - `map<string, vector<uint32>>`
  - `unordered_map<string, map<string, int32>>`
- If nested container support is needed later, it should use handwritten override of `Serialize` / `Deserialize` instead of expanding the default generator contract immediately.
- Unsupported types must fail generation loudly.

## 5. Generated C++ Output

### 5.1 Packet Interface
- Generated packets derive from common packet interface.
- Current common contract:
  - `GetOpcode()`
  - `virtual void Serialize(FPacketWriter& writer) const`
  - `virtual bool Deserialize(FPacketReader& reader)`

### 5.2 Why Virtual
- Default generated implementation should work immediately.
- If a packet needs custom behavior later, developer can override.
- Build should still pass even when only declarations/default bodies are used.

### 5.3 Handler Output
- Generator also emits handler interface/base skeleton.
- It includes:
  - per-opcode virtual handler declarations
  - dispatcher skeleton
  - safe default `false` handling for unimplemented handlers

## 6. Runtime Flow
- Send path:
  - generated packet object
  - `Serialize`
  - `FPacketWriter`
  - content payload
  - `NetworkLib` framing/cipher/send
- Receive path:
  - recv buffer
  - framing/cipher/checksum
  - `ContentHeader`
  - generated packet object
  - `Deserialize`
  - handler dispatch

## 7. Manual Generation Policy

### 7.1 Why Not Always Generate On Build
- C# restore/build cost slows C++ iteration loop.
- Most normal C++ builds do not change packet schema.
- Always-running generation adds work even when schema is unchanged.

### 7.2 Adopted Policy
- Packet generation is **manual**.
- Developer runs `PacketGenerator` only when schema changes.
- Generated output is then used by normal C++ builds.
- `RefactoringServer.sln` does not automatically invoke `PacketGenerator`.

### 7.3 Standard Workflow
1. Edit `Packet/**/*.yaml`
2. Run packet generation manually
3. Check `Generated/Packets/...`
4. Build C++ solution

### 7.4 Tool Entry Point
- Tool project:
  - `RefactoringServer/Tools/PacketGenerator`
- Convenience scripts:
  - `RefactoringServer/scripts/Generate-Packets.ps1`
  - `RefactoringServer/scripts/Generate-Packets.cmd`

## 8. Example YAML

```yaml
content: Echo

messages:
  - name: Echo
    rq:
      opcode: 1000
      fields:
        - { name: message, type: string }
    rp:
      opcode: 1001
      fields:
        - { name: message, type: string }
    noti:
      opcode: 1002
      fields:
        - { name: message, type: string }
```

## 9. Validation Plan
- Generator fails if `rq`/`rp` pair rule is broken.
- Generator fails on unsupported type.
- Generated packets must round-trip `Serialize -> Deserialize`.
- Generated handler skeleton must compile without handwritten implementation.
- `EchoServer`/`EchoClient` must continue to pass runtime packet validation.
