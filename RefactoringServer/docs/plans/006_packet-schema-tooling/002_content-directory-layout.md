# Content Directory Layout Plan

## 1. Goal
- Content-related code and schema should be grouped by content category.
- A developer should be able to open one directory like `Chat` and immediately see related assets together.
- Packet schema layout should follow the same content grouping rule.

## 2. Directory Rule

### 2.1 Content Root
- Runtime content code will live under:
  - `RefactoringServer/Contents`

### 2.2 Schema Root
- Packet schema will live under:
  - `RefactoringServer/Packet`
- Each content has its own directory.

### 2.3 Generated Packet Root
- Generated packet output will live under:
  - `RefactoringServer/Generated/Packets`
- Output is grouped by content name.

## 3. Example Layout

```text
RefactoringServer/
  Contents/
    Chat/
      FChatContent.h
      FChatContent.cpp
      FChatRoomService.h
      FChatRoomService.cpp
    Echo/
      FEchoContent.h
      FEchoContent.cpp

  Packet/
    Chat/
      Chat.yaml
    Echo/
      Echo.yaml

  Generated/
    Packets/
      Chat/
        ChatPackets.h
        ChatPacketHandler.h
      Echo/
        EchoPackets.h
        EchoPacketHandler.h
```

## 4. Content File Grouping Policy
- One content directory contains packets and handlers that belong to the same gameplay category.
- For example, `Chat` directory contains:
  - `RoomEnter`
  - `RoomLeave`
  - `RoomChange`
  - `Chatting`
- These messages are not split into separate top-level directories.

## 5. Schema Grouping Policy
- YAML files follow the same grouping rule as runtime content.
- `Chat` related packets are defined in `Packet/Chat/Chat.yaml`.
- `Echo` related packets are defined in `Packet/Echo/Echo.yaml`.
- Generator scans schema directories recursively.

## 6. Why This Structure
- Related content is easier to discover.
- Packet schema and runtime handler stay conceptually aligned.
- Future content modules like `Login`, `Inventory`, `World`, `Guild` can follow the same rule.

## 7. Current Adoption
- `Echo` schema should be moved from root schema directory into `Packet/Echo/Echo.yaml`.
- `PacketGenerator` should scan `Packet` recursively instead of only top-level files.
- `Contents` root should be created now even if full content classes are added later.
