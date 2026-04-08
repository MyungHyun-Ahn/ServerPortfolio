# 005 Benchmark Current

Status: Active  
Canonical: Yes  
Last Updated: 2026-04-08  
Scope: Benchmark execution and artifact rules

## 1. 현재 기준 실행기
- 반복 실험과 성능 비교는 [scripts/bench](D:\Project\ServerPortfolio\RefactoringServer\scripts\bench) 기준으로 통일한다.
- 실행 진입점은 [Run-Benchmark.ps1](D:\Project\ServerPortfolio\RefactoringServer\scripts\bench\Run-Benchmark.ps1)와 [Run-Benchmark.cmd](D:\Project\ServerPortfolio\RefactoringServer\scripts\bench\Run-Benchmark.cmd)다.
- 개별 `Run-*`, `Start-*` legacy 스크립트는 제거했다.

## 2. manifest 기준
- 시나리오 정의는 `YAML manifest`로 관리한다.
- `ChattingServer` 기본 예시는 [chatting-smoke.yaml](D:\Project\ServerPortfolio\RefactoringServer\scripts\bench\manifests\chatting-smoke.yaml)과 [chatting-rio-vs-iocp-128b-1h.yaml](D:\Project\ServerPortfolio\RefactoringServer\scripts\bench\manifests\chatting-rio-vs-iocp-128b-1h.yaml)이다.

## 3. 출력 규칙
- 모든 벤치 결과는 [Out/bench](D:\Project\ServerPortfolio\RefactoringServer\Out\bench) 아래에 둔다.
- 프로젝트 exe/lib는 `Out/<ProjectName>/`에 둔다.
- 성공 run에서는 client stdout/stderr 로그를 남기지 않고, 실패 run에서만 유지한다.

## 4. 현재 자주 보는 지표
- `sequence-summary.csv`
- 각 run의 `run-summary.json`
- `rtt.csv`
- `server.stdout.log`

## 5. 현재 주의점
- 같은 머신 `Echo` 벤치와 `Chatting` end-to-end 벤치를 같은 의미로 직접 비교하지 않는다.
- `Chat RTT`는 순수 네트워크 ping이 아니라 dummy event 처리 지연까지 포함된 app-level completion time으로 해석한다.

## 6. 관련 핵심 문서
- [001_powershell-benchmark-runner-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\011_benchmark_runner\001_powershell-benchmark-runner-plan.md)
- [021_chattingserver-128b-10m-backend-benchmark-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\021_chattingserver-128b-10m-backend-benchmark-review.md)
- [022_echo-server-windowsserver-2h-4mode-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\022_echo-server-windowsserver-2h-4mode-review.md)
- [scripts/bench/README.md](D:\Project\ServerPortfolio\RefactoringServer\scripts\bench\README.md)
