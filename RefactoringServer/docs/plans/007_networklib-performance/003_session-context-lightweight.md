# Session Context Lightweight Plan

## 1. 목적
- 세션과 콘텐츠 연결 비용을 줄인다.
- 한 연결당 초기화/종료/dispatch 비용을 낮춘다.

## 2. 현재 관찰점
- `FIocpServer`는 세션 슬롯과 generation 기반으로 세션을 관리한다.
- `EchoServer`는 로그인 상태를 `unordered_map + mutex`로 별도 관리한다.
- packet dispatch는 generated router를 통해 콘텐츠 핸들러로 간다.

## 3. 개선 후보

### 3.1 세션 접근 경량화
- hot field 배치를 점검한다.
- 자주 읽는 필드:
  - sessionId
  - closing
  - recv context
  - send state

### 3.2 콘텐츠 연결 비용
- 샘플 서버의 로그인 상태 저장을 더 가볍게 가져갈 수 있는지 검토
- 세션별 콘텐츠 포인터/컨텍스트 슬롯 방식도 후보로 본다.

### 3.3 종료 경로
- `CloseSession -> ReleaseSession` 경로에서 불필요한 작업이 없는지 확인
- 로그 출력, 상태 제거, queue flush 비용을 분리해서 본다.

## 4. 1차 범위
- 세션 클래스 재배치
- 콘텐츠 상태 저장 방식 후보 비교
- 로그인/에코 샘플에 과한 mutex 경로가 있는지 확인

## 5. 성공 기준
- 세션 수 증가 시 accept/close 비용이 덜 흔들린다.
- 재접속 시나리오에서 CPU 스파이크가 줄어든다.
