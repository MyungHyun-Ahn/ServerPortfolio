# Scripts Directory Guide

`scripts` 루트에는 폴더 단위 진입점만 둔다.

## Directory Layout

- `bench/`
  - 반복 실험, 스모크, 장기 부하 테스트
- `generate/`
  - packet/config/codegen 생성 스크립트

## Rules

- 반복 실험은 `scripts/bench/` 아래 manifest 기반으로 실행한다.
- 생성 작업은 `scripts/generate/` 아래 entrypoint를 사용한다.
- `Run-*`, `Start-*` 같은 일회성 루트 스크립트는 새로 만들지 않는다.
