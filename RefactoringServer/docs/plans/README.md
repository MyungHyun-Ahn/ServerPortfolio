# Plans Guide

## 1. Naming Rule
- `plans` is organized by work category directory.
- Each work directory starts with a numeric prefix like `001_`, `002_`.
- Documents inside each directory also start with numeric prefixes like `001_`, `002_`.
- New follow-up work in the same topic should stay in the same numbered directory.

## 2. Current Structure
- [001_foundation](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\001_foundation)
- [002_legacy-mhlib](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\002_legacy-mhlib)
- [003_networklib-crypto](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\003_networklib-crypto)
- [004_networklib-session](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\004_networklib-session)
- [005_networklib-packet-view](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\005_networklib-packet-view)
- [006_packet-schema-tooling](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\006_packet-schema-tooling)

## 3. Writing Rule
- A plan should fix direction and boundary before implementation starts.
- Prefer task-oriented naming over date-oriented naming.
- If execution policy matters, write it explicitly in the plan.

## 4. Packet Generation Policy
- `PacketGenerator` is a manual tool.
- It is not automatically invoked during normal C++ solution build.
- Standard entry points:
  - [Generate-Packets.ps1](D:\Project\ServerPortfolio\RefactoringServer\scripts\Generate-Packets.ps1)
  - [Generate-Packets.cmd](D:\Project\ServerPortfolio\RefactoringServer\scripts\Generate-Packets.cmd)

## 5. Content Grouping
- Content, schema, and generated packet output are grouped by content category directory.
- Reference:
  - [002_content-directory-layout.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\006_packet-schema-tooling\002_content-directory-layout.md)
