# Mailbox Owner Transfer Delegate / Work Stealing Review

## 1. 목적
- 현재 구현된 `delegate / work stealing`이 어떤 메커니즘 위에서 동작하는지 정리한다.
- 과거 worker-global queue 기반 시도와 현재 구조의 차이를 분명히 남긴다.

## 2. 결론
- 현재 `delegate / work stealing`은 `mailbox owner transfer`다.
- queue item을 옮기지 않는다.
- content mailbox의 consumer owner worker만 바꾼다.

## 3. trigger 의미
### delegate
- 바쁜 source worker가 특정 content를 다른 worker로 밀어내는 push trigger

### work stealing
- idle worker가 바쁜 worker의 content를 하나 가져오는 pull trigger

### 공통점
- 둘 다 runtime 내부 정책이다.
- 외부 content 코드가 직접 worker를 고르지 않는다.
- 실제 commit은 work boundary에서만 일어난다.

## 4. 메커니즘
### 4-1. request
- runtime이 `requestedTransferTargetWorkerIndex`를 기록한다.
- 이 단계에서는 queue를 건드리지 않는다.

### 4-2. boundary commit
- worker가 현재 callback 또는 frame을 끝낸 직후 boundary에 도달하면 commit을 시도한다.
- 이때
  - source detach
  - target attach
  - slot.worker / workerIndex 갱신
  - execution state owner 갱신
  - session route worker cache 갱신
가 수행된다.

### 4-3. single-consumer
- old worker가 더 이상 consume하지 않은 뒤 target worker만 이어서 consume한다.
- mailbox가 비어 있을 필요는 없다.
- queue는 그대로 두고 consumer만 바꾼다.

## 5. 정책
- allowList content만 대상
  - 현재 `Room`만 허용
- request cooldown
- commit cooldown
- source worker stabilization window
- 중복 request 금지

## 6. 핵심 안전 가드
### 6-1. boundary-only commit
- callback/frame 도중 transfer commit 금지

### 6-2. pending move source/target transfer 금지
- move가 걸린 source content와 pending target content는 transfer 후보에서 제외
- request 시점과 commit 시점 모두 재검사

이 가드가 없으면
- move completion이 old worker 가정으로 commit하려는 순간
- owner transfer가 source/pendingTarget의 worker 캐시를 바꿔버릴 수 있다.

### 6-3. held packet 보존
- move 중 packet은 `pendingPackets`에 hold하고 route commit 뒤 replay한다.
- transfer는 이 경로를 깨면 안 된다.

## 7. 실패 원인과 수정
실패의 직접 원인은 `Pending move`와 `owner transfer`가 충돌한 것이었다.
- 증상
  - `move route commit ... committed=0`
  - 이후 held packet 고착
  - client `echo-response timeout`
- 수정
  - pending move 관련 content transfer 금지
  - request/commit 양쪽에서 재검사

## 8. 검증
### 3분 loaded run
- [owner_transfer_policy_loaded_3m](D:\Project\ServerPortfolio\RefactoringServer\Out\owner_transfer_policy_loaded_3m)
- 실제
  - `delegate request scheduled`
  - `work steal request scheduled`
  - `owner transfer commit`
  발생
- client 성공 종료

### 10분 loaded run
- [owner_transfer_policy_loaded_10m](D:\Project\ServerPortfolio\RefactoringServer\Out\owner_transfer_policy_loaded_10m)
- 같은 조건에서 실제 transfer가 반복적으로 발생해도 성공 종료
- `move route commit committed=0` 재현 없음

## 9. 리뷰 결론
- 현재 `delegate / work stealing`은 더 이상 실험성 아이디어가 아니라, mailbox 구조 위에 붙은 실제 load-balancing 기능으로 본다.
- 신뢰의 근거는 “정책이 발동한다”가 아니라, “move와 함께 돌아도 3분/10분 고부하 런을 통과한다”는 점이다.
