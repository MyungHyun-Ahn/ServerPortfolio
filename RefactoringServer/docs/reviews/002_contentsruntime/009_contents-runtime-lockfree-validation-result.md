# ContentsRuntime lock-free inbox 6시간 안정성 검증 결과

## 1. 목적
- `lock-free packet inbox` 프로토타입이 실제로 장시간 안정적으로 동작하는지 확인한다.
- 이번 검증은 성능 측정보다 안정성 검증이 목적이다.

## 2. 실행 조건
- 실행 스크립트:
  - [Run-ContentsRuntimeRaceValidation.ps1](D:\Project\ServerPortfolio\RefactoringServer\scripts\Run-ContentsRuntimeRaceValidation.ps1)
- 조건:
  - `DurationSeconds=21600`
  - `Sessions=100`
  - `Count=1`
  - `IntervalMs=200`
  - `PacketsPerSend=2`
  - `RecvTimeoutMs=5000`
  - `RaceMode=sleep0`
  - `RacePeriod=1`
  - `--contents-fail-fast` 활성화

## 3. 로그 경로
- 런처 로그
  - [launcher_6h_20260403_032550.log](D:\Project\ServerPortfolio\RefactoringServer\Out\contents-race-validation\launcher_6h_20260403_032550.log)
- 서버 로그
  - [server_20260403_032550.log](D:\Project\ServerPortfolio\RefactoringServer\Out\contents-race-validation\server_20260403_032550.log)
  - [server_20260403_032550.err.log](D:\Project\ServerPortfolio\RefactoringServer\Out\contents-race-validation\server_20260403_032550.err.log)
- 클라이언트 로그
  - [client_20260403_032550.log](D:\Project\ServerPortfolio\RefactoringServer\Out\contents-race-validation\client_20260403_032550.log)
  - [client_20260403_032550.err.log](D:\Project\ServerPortfolio\RefactoringServer\Out\contents-race-validation\client_20260403_032550.err.log)

## 4. 결과 요약
- 런처 로그:
  - `ContentsRuntime race validation finished successfully.`
- 클라이언트 로그:
  - `echo validation succeeded. sessions=100 responses=10375394 ... holdSeconds=21600`
- 서버 로그:
  - 종료 직전까지 `sessions=100` 유지 구간에서
    - `recvTPS` 약 `470~500`
    - `sendTPS` 약 `470~500`
    - `enqueueFailTPS=0`
    - `echoPacketEnqueueLockUs=0.00`
  - 종료 시점에는 세션이 정상적으로
    - `client disconnected`
    - `echo content leave`
    - `Session closed`
    순서로 정리됨
- 에러 로그:
  - 서버/클라이언트 에러 로그 모두 비어 있음

## 5. 로그 해석
- `enqueueFailTPS=0`
  - 라우팅 실패나 queue 삽입 실패는 관찰되지 않았다.
- `echoPacketEnqueueLockUs=0.00`
  - packet inbox 락 경합은 측정상 사라졌다.
- `echoQueue`는 중간에 오르내렸지만 `echoMaxQueue=86` 범위 내에서 소화되었다.
- `echoLastDelayFrame=1`, `echoMaxDelayFrame=1`
  - 프레임 지연이 누적되는 증상은 없었다.

## 6. 이번 결과로 말할 수 있는 것
- `lock-free packet inbox` 프로토타입은 현재 조건에서 6시간 동안 안정적으로 동작했다.
- race injection(`sleep0`)을 켠 상태에서도
  - 즉시 크래시
  - 데드락
  - bootstrap timeout
  - 세션 종료 누락
  은 관찰되지 않았다.

## 7. 아직 남는 범위
- 이번 검증은 `100세션`, `IntervalMs=200`, `PacketsPerSend=2` 조건이다.
- 더 공격적인 stress 조건
  - `IntervalMs=0`
  - 더 큰 `PacketsPerSend`
  - reconnect 확률 증가
  는 별도 검증 대상으로 남길 수 있다.
- 다만 현재 프로젝트 단계에서는 이번 6시간 검증만으로도 `프로토타입 채택` 판단엔 충분한 근거가 된다.

## 8. 결론
- 현재 기준으로 `lock-free inbox`는 유지해도 좋다.
- 토글은 남겨두되, 기본 경로로 유지하는 쪽이 합리적이다.
