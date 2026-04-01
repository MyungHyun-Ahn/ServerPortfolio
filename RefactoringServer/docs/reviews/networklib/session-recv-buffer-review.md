# NetworkLib Session Recv Buffer Review

## 1. 범위
- [`FRecvBuffer.h`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FRecvBuffer.h)
- [`FPacketView.h`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FPacketView.h)
- [`IPacketFramer.h`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\IPacketFramer.h)
- [`FDefaultPacketFramer.cpp`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FDefaultPacketFramer.cpp)
- [`FSession.h`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FSession.h)
- [`FSession.cpp`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FSession.cpp)
- [`FIocpServer.cpp`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FIocpServer.cpp)
- [`IApplicationHandler.h`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\IApplicationHandler.h)

## 2. 변경 목적
- 이전 recv 경로는 `WSARecv -> 임시 vector -> 세션 누적 vector` 순서로 한 번 더 복사했다.
- 레거시 프로젝트의 강점은 세션이 recv ring buffer를 직접 소유하고, `WSARecv`가 그 free 영역으로 바로 들어간다는 점이었다.
- 이번 변경은 그 의도를 현재 `PacketFramer` 구조에 맞게 옮기고, 가능하면 payload를 별도 버퍼로 복사하지 않고 view로 넘기는 것이 목적이다.

## 3. 설계 요약
### 3-1. 세션 소유 recv ring buffer
- [`FSession`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FSession.h)은 [`FRecvBuffer`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FRecvBuffer.h)를 소유한다.
- `FRecvBuffer`는 `readOffset`, `writeOffset`, `usedSize` 기반의 고정 용량 ring buffer다.
- `BuildRecvWsabufs()`는 free 영역을 최대 2개 `WSABUF`로 나눠 `WSARecv`에 넘긴다.

### 3-2. direct recv
- [`FIocpServer::PostRecv()`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FIocpServer.cpp)은 더 이상 임시 recv vector를 만들지 않는다.
- 세션 recv ring buffer free 영역을 바로 `WSARecv` 대상 버퍼로 사용한다.
- 완료 후 [`FSession::CommitRecvBytes()`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FSession.cpp)로 실제 수신 길이만 반영한다.

### 3-3. framer의 recv buffer 지원
- [`IPacketFramer`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\IPacketFramer.h)는 `std::vector<char>`뿐 아니라 [`FRecvBuffer`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FRecvBuffer.h)에서도 패킷을 추출할 수 있게 됐다.
- [`FDefaultPacketFramer`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FDefaultPacketFramer.cpp)는 recv buffer에서 헤더를 `Peek`하고, 필요하면 `EnsureContiguous()`로 현재 패킷 구간만 선형화한다.

### 3-4. packet view 기반 dispatch
- [`FPacketView`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FPacketView.h)는 `opcode`, `randomKey`, `checkSum`, `payload pointer`, `payloadLength`를 담는 얇은 view 타입이다.
- [`FDefaultPacketFramer::TryExtractPacketView()`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FDefaultPacketFramer.cpp)는 payload를 별도 `std::vector<char>`로 복사하지 않고 recv buffer 내부 포인터를 view로 만든다.
- [`IApplicationHandler`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\IApplicationHandler.h)는 이제 `const Packet::FPacketView&`를 받아, 현재 콜백 범위 안에서만 유효한 payload view를 사용한다.
- 콜백이 끝난 뒤에는 [`FIocpServer.cpp`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FIocpServer.cpp)가 `Discard()`로 해당 패킷 길이만큼 recv buffer를 전진시킨다.

## 4. 판단 근거
- 레거시 [`CNetSession::PostRecv()`](D:\Project\ServerPortfolio\NetworkLib\CNetServer.cpp), [`CNetSession::RecvCompleted()`](D:\Project\ServerPortfolio\NetworkLib\CNetServer.cpp)는 ring buffer direct recv와 view 지향 설계라는 점에서 여전히 참고 가치가 있었다.
- 다만 레거시처럼 세션이 체크섬, 암복호화, 콘텐츠 큐까지 전부 끌어안으면 현재 계층 분리 방향과 충돌한다.
- 그래서 이번에는 "recv ring buffer와 payload view"까지만 세션/패킷 계층에 가져오고, 해석과 dispatch 경계는 framer와 application handler에 남겨뒀다.

## 5. 확인된 사실
- [`LockFreeTests.exe`](D:\Project\ServerPortfolio\RefactoringServer\Out\LockFreeTests.exe)에서 아래 항목 PASS
  - `Packet framer recv buffer`
  - `Packet framer packet view`
- 기존 테스트도 전부 PASS
  - `Queue linear FIFO`
  - `Queue parallel sum`
  - `Stack parallel sum`
  - `TLS memory pool parallel`
  - `Packet cipher round trip`
  - `Null packet cipher`
  - `Packet framer round trip`
  - `Packet framer partial receive`
- [`EchoServer.exe`](D:\Project\ServerPortfolio\RefactoringServer\Out\EchoServer.exe) `--headless` + [`EchoClient.exe`](D:\Project\ServerPortfolio\RefactoringServer\Out\EchoClient.exe)
  - `--count 8`
  - `--payload-size 48`
  - `--send-chunk-size 5`
  - `--send-chunk-delay-ms 1`
  - `--recv-buffer-size 11`
  - 위 조건에서 `echo validation succeeded.` 확인

## 6. 현재 한계
- recv ring buffer 용량은 현재 `max(recvBufferSize * 8, 65536)` 기준이다. 임시 기준이므로 운영 기준 용량은 이후 조정이 필요하다.
- `PacketFramer`가 없는 경로는 현재 ring buffer recv에서 지원하지 않는다. 지금 구조는 framer 기반 서버를 전제로 한다.
- payload가 wrap된 경우 `EnsureContiguous()`가 현재 사용 중 데이터 일부를 버퍼 내부에서 선형화한다. 즉 별도 payload 버퍼 복사는 없어졌지만, wrap 상황의 내부 정렬 복사는 여전히 존재할 수 있다.
- `FPacketView`는 콜백 범위 안에서만 유효하다. 콘텐츠 계층이 오래 보관하려면 직접 복사해야 한다.

## 7. 다음 확인 항목
- packet dispatcher 계층이 들어왔을 때 `FPacketView`를 그대로 넘길지, 별도 content header view를 둘지 검토
- 세션 종료 직전 recv overflow, malformed packet 상황 추가 테스트
- 장시간 soak 조건에서 recv ring buffer 경로 검증
