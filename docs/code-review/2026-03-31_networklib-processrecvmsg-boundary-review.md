# 코드 리뷰 노트

## 1. 작업 개요
- 작업명: `NetworkLib` `CBaseContents::ProcessRecvMsg()` 의존 관계 및 민감 구간 정리
- 관련 문서:
  - `docs/design/2026-03-31_networklib-game-server-refactoring-plan.md`
  - `docs/code-review/2026-03-31_networklib-session-guard-refactoring-review.md`
  - `docs/code-review/2026-03-31_networklib-stop-shutdown-review.md`
- 대상 파일:
  - `NetworkLib/CBaseContents.cpp`
  - `NetworkLib/CBaseContents.h`
  - `NetworkLib/CNetServer.h`
- 목적:
  - `ProcessRecvMsg()`가 어떤 런타임 의존을 갖는지 정리하고,
  - 작은 리팩터링처럼 보이는 변경이 왜 로그인 응답 실패로 이어졌는지 현재 확인 가능한 사실 기준으로 남기며,
  - 다음 단계 리팩터링의 안전 경계를 문서로 먼저 확정한다.

## 2. 현재 구조 요약
- `CBaseContents::ProcessRecvMsg()`는 `m_umapSessions`를 순회하면서 세션 ID별로 `g_NetServer->AcquireSession()`을 호출한다.
- 세션 획득에 성공하면 `pSession->m_RecvMsgQueue`의 현재 사용량을 읽고, 그 수만큼 메시지를 dequeue하여 `OnRecv()`에 전달한다.
- `OnRecv()` 결과가 `RECV_MOVE`이면 현재 메시지를 해제하고 반복을 중단한다.
- `OnRecv()` 결과가 `RECV_FALSE`이면 `g_NetServer->Disconnect(sessionId)`를 호출한 뒤 메시지를 해제하고 반복을 중단한다.
- 읽을 메시지가 하나라도 있었으면 마지막에 `g_NetServer->SendPQCS(pSession)`를 호출해 다음 처리 경로를 다시 깨운다.
- 마지막에는 `m_iIOCountAndRelease`를 감소시키고 `0`이면 `ReleaseSession()`을 호출한다.

## 3. 확인된 의존 관계
### 3-1. 세션 참조 카운트 의존
- `ProcessRecvMsg()`는 세션을 단순 조회하는 함수가 아니라, `AcquireSession()`과 짝을 이루는 release 책임까지 함께 가진다.
- 즉 `ProcessRecvMsg()` 본문은 "수신 처리"와 "세션 수명주기 정리"가 한 함수 안에 결합돼 있다.

### 3-2. 콘텐츠 콜백 의존
- `OnRecv()`는 가상 함수이므로 실제 동작은 콘텐츠 구현체에 따라 달라진다.
- 현재 Echo 계열 콘텐츠는 로그인 응답, 에코 응답, 콘텐츠 이동 같은 흐름을 `OnRecv()` 내부에서 직접 결정한다.
- 따라서 `ProcessRecvMsg()`는 단순 큐 소비 함수가 아니라, 콘텐츠 상태 전이의 진입점 역할도 겸한다.

### 3-3. 재스케줄 의존
- `recvMsgCount != 0`일 때 호출하는 `SendPQCS(pSession)`는 한 번의 루프 처리 이후 다음 수신 처리가 이어지게 만드는 재기동 신호 역할을 한다.
- 이 호출 시점이 바뀌거나 호출 주체가 바뀌면, 다음 메시지 처리 타이밍이 달라질 가능성이 있다.

### 3-4. Disconnect 경로 의존
- `RECV_FALSE` 분기에서는 현재 함수가 직접 `Disconnect(sessionId)`를 호출한다.
- 이 경로는 메시지 해제, 세션 release, 이후 leave 처리와 순서적으로 맞물릴 수 있으므로, 단순 helper 추출이라도 호출 경계가 바뀌면 영향 범위를 다시 확인해야 한다.

## 4. 민감 구간
### 4-1. `OnRecv()` 호출 전후
- `OnRecv()`는 콘텐츠 구현에 따라 현재 콘텐츠 교체, 세션 종료, 응답 송신 같은 부수효과를 만들 수 있다.
- 따라서 이 호출을 helper로 감싸거나 다른 클래스로 이동할 때는 "호출 위치가 같다"만으로 안전하다고 보기 어렵다.

### 4-2. `SendPQCS()` 호출 위치
- 현재는 세션별 메시지 처리 루프가 끝난 뒤 같은 함수 안에서 호출된다.
- 이 호출이 helper 내부로 들어가거나 외부로 나가면, 호출 시점과 호출 당시의 세션 상태가 달라질 수 있다.

### 4-3. `releaseSession` 시점
- `ProcessRecvMsg()`는 세션별 처리 후 항상 release를 수행한다.
- helper 추출 시 이 release 책임이 분산되면, `AcquireSession()`과 짝이 어긋나거나 중복 release 위험이 생길 수 있다.

### 4-4. `m_RecvMsgQueue.GetUseSize()` 기반 반복
- 현재 구조는 반복 시작 전에 큐 길이를 읽고 그 수만큼만 dequeue한다.
- 처리 도중 추가 메시지가 들어오더라도 같은 루프에서 전부 소비하지 않고, `SendPQCS()`로 다음 턴을 예약하는 형태다.
- 이 전제는 처리 경계가 바뀌면 쉽게 깨질 수 있다.

## 5. 실패 재현 기록
- 시도한 변경:
  - `ProcessRecvMsg()` 내부의 세션별 수신 처리 본문을 로컬 helper(`processRecvSession`)로 분리
  - 호출 경계는 `CBaseContents.cpp` 내부로 유지
  - 외부 인터페이스는 변경하지 않음
- 빌드 결과:
  - `Portfolio.sln` x64 Debug 빌드 성공
- 런타임 결과:
  - `EchoServer` 실행 후 `TestClient --clients 1 --repeat 1 --hold-seconds 1 --heartbeat-ms 1000 --no-wait` 수행
  - 클라이언트 접속 성공 후 `로그인 응답 수신 실패` 발생
  - 같은 변경을 되돌리면 동일 조건에서 로그인/에코/heartbeat가 다시 성공

## 6. 현재 시점에서 확정 가능한 사실
- `ProcessRecvMsg()`는 보기보다 민감한 함수이며, 단순한 본문 추출 수준의 정리도 런타임 회귀를 만들 수 있다.
- 회귀는 컴파일 오류가 아니라 실제 로그인 응답 미수신으로 관찰됐다.
- 동일 테스트 조건에서 원복 후에는 정상 동작했으므로, 문제는 테스트 환경이 아니라 해당 리팩터링 시도와 연관된 것으로 봐야 한다.

## 7. 불확실한 가설
- helper 추출 자체가 문제인지, lambda 캡처와 지역 변수 배치 변화가 문제인지, 또는 `SendPQCS()`와 release 시점의 미세한 실행 순서 차이가 문제인지는 아직 확정되지 않았다.
- 현재 로그만으로는 로그인 응답 미수신의 직접 원인을 특정할 근거가 부족하다.
- 따라서 "왜 실패했는지"보다 "어디까지가 안전 경계인지"를 먼저 문서화하는 편이 타당하다.

## 8. 다음 작업 기준
- `ProcessRecvMsg()`는 당분간 호출 경계와 본문 구조를 그대로 유지한다.
- 이 함수에 대한 다음 변경은 아래 중 하나만 허용하는 것이 안전하다.
  - 코드 변경 없이 호출 관계, 부수효과, 테스트 시나리오를 문서화
  - 로그 추가처럼 실행 순서를 바꾸지 않는 관측성 보강
  - `AcquireSession()`과 release 짝을 유지한 상태에서 한 줄 단위의 매우 작은 정리
- `CBaseContents` 계층 정리는 `ProcessMoveJob()`처럼 이미 통과한 경로부터 우선 진행하고, `ProcessRecvMsg()`는 별도 검증 계획을 세운 뒤 다시 다룬다.

## 9. 회귀 테스트 기준선 확인
- 실행 날짜:
  - 2026-03-31
- 실행 항목:
  - `Portfolio.sln` x64 Debug 빌드
  - `TestClient --clients 1 --repeat 1 --hold-seconds 1 --heartbeat-ms 1000 --no-wait`
  - `TestClient --clients 10 --repeat 5 --hold-seconds 1 --heartbeat-ms 1000 --no-wait`
  - `TestClient --clients 1 --repeat 5 --hold-seconds 3 --heartbeat-ms 1000 --no-wait`
- 관찰 결과:
  - 빌드 성공
  - 단일 클라이언트 로그인/에코/heartbeat 성공
  - 다중 클라이언트 로그인 및 에코 반복 성공
  - 재스케줄 확인용 반복 에코와 heartbeat 성공
  - 모든 테스트에서 최종 출력 `모든 테스트가 성공했습니다.`
- 해석:
  - 현재 커밋 기준 코드는 `ProcessRecvMsg()` 관련 회귀 테스트 묶음의 기준선을 만족한다.
  - 이후 이 경로를 수정하면 같은 테스트를 다시 실행해 비교해야 한다.

## 10. 결론
### 확정된 사실
- `ProcessRecvMsg()`는 세션 수명주기, 콘텐츠 상태 전이, PQCS 재스케줄, disconnect 경로가 결합된 민감 구간이다.
- 외형상 작은 리팩터링도 로그인 응답 실패를 유발할 수 있다는 것이 테스트로 확인됐다.

### 불확실한 부분
- 실패의 직접 원인은 아직 특정되지 않았다.
- 현재 정보만으로 helper 추출 자체를 일반적인 금지 규칙으로 단정할 수는 없다.

### 다음 확인 항목
- `ProcessRecvMsg()` 경로에 관측용 로그를 어디까지 넣을지 결정
- Echo 콘텐츠의 `OnRecv()`가 만드는 부수효과를 호출 순서 기준으로 문서화
- `ProcessRecvMsg()`를 다시 다룰 때 사용할 회귀 테스트 묶음을 별도 정의
