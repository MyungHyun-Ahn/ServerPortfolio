# ContentsRuntime 트러블슈팅

## 1. 증상
- `ContentsRuntime` 적용 후 단발 경로는 통과했지만, [`EchoClient.exe`](D:\Project\ServerPortfolio\RefactoringServer\Out\EchoClient.exe)에서 `--hold-seconds 1` 같은 반복 시나리오가 종료되지 않았다.
- 서버 로그를 보면 첫 `Login -> Chat snapshot -> Echo`는 정상 처리되지만, 이후 세션이 계속 열린 채 idle 상태로 남아 있었다.

## 2. 재현 조건
- 서버:
  - [`EchoServer.exe`](D:\Project\ServerPortfolio\RefactoringServer\Out\EchoServer.exe) `--headless --log-packets`
- 클라이언트:
  - [`EchoClient.exe`](D:\Project\ServerPortfolio\RefactoringServer\Out\EchoClient.exe) `--sessions 1 --count 1 --response-thread-count 1 --responses-per-thread 1 --hold-seconds 1 --interval-ms 200 --quiet`

## 3. 원인
- 클라이언트의 반복 루프는 매 사이클마다 아래 순서를 다시 수행하고 있었다.
  1. `LoginRq`
  2. `RoomSnapshotRq`
  3. `EchoRq`
- 하지만 `ContentsRuntime` 구조에서는 로그인 성공 후 세션이 `AuthContent`에서 `EchoContent`로 이동한다.
- 즉 같은 TCP 연결을 유지한 상태에서 다음 사이클에 다시 `LoginRq`를 보내면, 세션은 이미 `EchoContent`에 있으므로 `LoginRq`를 처리하지 않는다.
- 그 결과 클라이언트는 `LoginRp`를 기다리며 멈추고, 서버는 해당 패킷을 무시한 채 세션을 유지하게 된다.

## 4. 수정 내용
- [`EchoClient/Main.cpp`](D:\Project\ServerPortfolio\RefactoringServer\EchoClient\Main.cpp)에 `requiresBootstrapAfterConnect` 플래그를 추가했다.
- 새 동작은 아래와 같다.
  - 새 소켓 연결 직후에만 `LoginRq`와 `RoomSnapshotRq`를 보낸다.
  - 같은 연결을 유지하는 반복 루프에서는 `EchoRq`만 보낸다.
  - 재접속이 일어나면 다시 bootstrap 과정을 수행한다.

## 5. 수정 후 기대 동작
- 연결당 bootstrap은 1회만 수행된다.
- `holdSeconds > 0` 반복 시나리오에서도 이미 `EchoContent`로 이동한 세션이 다시 로그인 응답을 기다리며 멈추지 않는다.
- `reconnect` 옵션을 켠 경우에는 새 연결마다 bootstrap이 다시 수행된다.

## 6. 검증 포인트
- 단발 경로
  - `holdSeconds=0`
  - `echo validation succeeded.`
- 반복 경로
  - `holdSeconds=1`
  - 정상 종료 여부
  - 서버 로그에서 불필요한 `login succeeded` 반복이 사라졌는지 확인

## 7. 검증 결과
- `RefactoringServer.sln` x64 Debug 빌드 성공
- 단발 경로 재검증 성공
  - `echo validation succeeded. sessions=1 responses=1 ... holdSeconds=0`
- 반복 경로 재검증 성공
  - `echo validation succeeded. sessions=1 responses=6 ... holdSeconds=1`
- 서버 로그에서도 같은 연결에 대해
  - 최초 1회만 `login succeeded`
  - 최초 1회만 `chat snapshot served`
  - 이후 반복 구간은 `received ... opcode=1000`만 증가
  - 마지막에 정상 `client disconnected`
  흐름을 확인했다.

## 8. 메모
- 이 이슈는 `콘텐츠 이동을 도입했는데도 클라이언트가 연결 단위 bootstrap과 사이클 단위 요청을 구분하지 못한 것`이 핵심이었다.
- 이후 다른 콘텐츠 서버를 붙일 때도
  - "연결당 1회 수행"
  - "사이클마다 반복 수행"
  를 분리해서 설계하는 것이 중요하다.
