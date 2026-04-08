# Generate Scripts Guide

생성 계열 스크립트는 `scripts/generate/` 아래에 모아둔다.

## Entry Points

- `Generate-Packets.cmd`
- `Generate-Packets.ps1`
- `Generate-Configs.cmd`
- `Generate-Configs.ps1`
- `Generate-Codegen.cmd`
- `Generate-Codegen.ps1`

## Usage

- packet schema만 갱신할 때:
  - `scripts\\generate\\Generate-Packets.cmd`
- config schema만 갱신할 때:
  - `scripts\\generate\\Generate-Configs.cmd`
- packet/config를 한 번에 갱신할 때:
  - `scripts\\generate\\Generate-Codegen.cmd`
