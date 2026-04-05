# IOCP AcceptEx Reconnect Stress Review

## 1. 목적
- `FIocpServer`의 `AcceptEx` 전환 이후, 재접속 churn이 큰 상황에서도 accept path가 안정적으로 유지되는지 확인한다.
- 실패가 발생하더라도 원인이 `AcceptEx` 서버 경로인지, 아니면 로컬 테스트 환경/클라이언트 한계인지 분리한다.
- 현재 구현에 accept slot pool이 실제로 적용되어 있는지, 그리고 레거시의 accept session pool과 어떤 차이가 있는지 정리한다.

## 2. 대상 파일
- IOCP backend
  - [FIocpServer.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.h)
  - [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.cpp)
- 기준 흐름 문서
  - [016_iocp-echo-server-flow-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\016_iocp-echo-server-flow-review.md)

## 3. 테스트 시나리오
### 3-1. 실패 시나리오 1
- 경로: [iocp_acceptex_reconnect_100x3m](D:\Project\ServerPortfolio\RefactoringServer\Out\iocp_acceptex_reconnect_100x3m)
- 조건
  - `100 sessions`
  - `3 minutes`
  - `reconnectProbabilityPercent=100`
  - `reconnectDelayMs=0`
  - `roomCount=20`
- 결과
  - 클라이언트 실패: `session[0] failed: no joinable room available.`
- 해석
  - 이 케이스는 accept path 문제가 아니라 room 수용량 부족이다.

### 3-2. 실패 시나리오 2
- 경로: [iocp_acceptex_reconnect_100x3m_room50](D:\Project\ServerPortfolio\RefactoringServer\Out\iocp_acceptex_reconnect_100x3m_room50)
- 조건
  - `100 sessions`
  - `3 minutes`
  - `reconnectProbabilityPercent=100`
  - `reconnectDelayMs=0`
  - `roomCount=50`
- 결과
  - 클라이언트 실패: `session[0] failed: connect failed: 10048`
- 해석
  - 서버 accept path보다 클라이언트 로컬 포트 재사용 한계가 먼저 터진 케이스다.
  - 서버 로그에는 `AcceptEx completion failed`, `AcceptEx repost failed`, `SO_UPDATE_ACCEPT_CONTEXT failed`, `All session slots are in use`가 나타나지 않았다.

### 3-3. 실패 시나리오 3
- 경로: [iocp_acceptex_reconnect_50x3m_delay10](D:\Project\ServerPortfolio\RefactoringServer\Out\iocp_acceptex_reconnect_50x3m_delay10)
- 조건
  - `50 sessions`
  - `3 minutes`
  - `reconnectProbabilityPercent=100`
  - `reconnectDelayMs=10`
- 결과
  - 클라이언트 실패: `session[0] failed: connect failed: 10048`
- 해석
  - delay를 `10ms`로 늘려도 로컬 클라이언트 포트 한계가 먼저 드러났다.

### 3-4. 유효한 고빈도 재접속 검증
- 경로: [iocp_acceptex_reconnect_10x3m_delay250](D:\Project\ServerPortfolio\RefactoringServer\Out\iocp_acceptex_reconnect_10x3m_delay250)
- 조건
  - `10 sessions`
  - `3 minutes`
  - `reconnectProbabilityPercent=100`
  - `reconnectDelayMs=250`
  - `roomCount=20`
- 결과
  - 클라이언트 성공
  - [client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\iocp_acceptex_reconnect_10x3m_delay250\client.log)
    - `echo validation succeeded. sessions=10 responses=6700 ...`
  - 서버 로그 집계
    - `AcceptEx completion failed`: `0`
    - `AcceptEx repost failed`: `0`
    - `SO_UPDATE_ACCEPT_CONTEXT failed`: `0`
    - `All session slots are in use`: `0`
    - `Client connected | Session closed` 합계: `20100`
- 해석
  - `AcceptEx` 경로는 짧은 주기의 connect/disconnect churn을 정상적으로 처리했다.

## 4. 현재 accept slot pool 적용 여부
### 4-1. 결론
- 현재 `FIocpServer`에는 accept slot pool이 적용되어 있다.
- 다만 이 풀은 `accept context` 재사용 풀이고, 레거시에서 보던 의미의 `accepted socket까지 재사용하는 accept session pool`은 아니다.

### 4-2. 적용된 것
- `SAcceptContext[]` 고정 배열
- slot별 `OVERLAPPED`, address buffer, `acceptedSocket`, `slotIndex`
- 서버 시작 시 pre-post
- accept completion 후 같은 slot에 repost

### 4-3. 아직 적용되지 않은 것
- `acceptedSocket` 자체의 재사용
- 현재 `PostAccept()`는 매번 새 `WSASocketW(...)`를 만들고 이전 socket은 닫는다.

## 5. socket 재사용 판단
- 현재 판단은 `기본 채택하지 않음`이다.
- 이유
  - 직접 테스트 기준 차이가 크지 않았다.
  - accept 이후 병목은 보통 attach, bootstrap, send 경로에서 더 크게 보인다.
  - socket 재사용은 상태 초기화와 미완료 I/O 정리가 까다롭다.
- 결론
  - 지금은 `accept context slot pool`만 유지
  - `accepted socket reuse`는 필요 시 별도 옵션 실험으로만 재검토

## 6. 결론
- `AcceptEx` 전환 후 재접속이 잦은 상황에서도 서버 accept path는 정상 동작했다.
- 더 공격적인 조건에서 먼저 터진 것은 `AcceptEx`가 아니라 로컬 클라이언트 포트 재사용 한계였다.
- 현재 `FIocpServer`는 accept slot pool을 이미 갖고 있지만, 그 범위는 `accept context` 재사용까지다.
