# BuildPacket 복사 감소 계획

## 1. 목적
- `FDefaultPacketFramer::BuildPacket()` 경로에서 발생하는 불필요한 버퍼 복사를 줄인다.
- 현재의 `1-session 1 in-flight WSASend` 규칙은 유지한다.
- 성능 개선은 `WSASend 호출 수 증가`가 아니라 `단일 WSASend 안에서의 복사 감소` 방향으로 가져간다.

## 2. 현재 상태
- [FDefaultPacketFramer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FDefaultPacketFramer.cpp)의 `BuildPacket()`은 아래 순서로 동작한다.
  1. `SPacketHeader` 구성
  2. `outPacket.resize(header + payload)`
  3. 헤더 `memcpy`
  4. payload `memcpy`
- 즉 `payload`가 이미 준비된 상태여도, wire packet을 만들기 위해 한 번 더 큰 버퍼로 복사한다.
- 현재 `Send()` 경로에서도 content payload 조립 이후 다시 framed packet을 만드는 단계가 있으므로, 전체적으로는 복사 비용이 누적된다.

## 3. 유지해야 할 전제
- 세션당 동시에 `WSASend`는 1개만 허용한다.
- lock-free send queue 구조는 유지한다.
- packet header / checksum / randomKey 규칙은 유지한다.
- 기능 회귀 없이 `Login -> Echo`, 스트레스 테스트, packet generator 기반 경로가 그대로 동작해야 한다.

## 4. 레거시 참고 기준
- 조각 버퍼 수명 관리는 레거시 `NetworkLib`의 send completion 패턴을 참고한다.
- 주요 참고 위치:
  - [CNetServer.cpp](D:\Project\ServerPortfolio\NetworkLib\CNetServer.cpp)
  - [CNetServer.h](D:\Project\ServerPortfolio\NetworkLib\CNetServer.h)
  - [COverlappedAllocator.h](D:\Project\ServerPortfolio\includes\NetworkLib\COverlappedAllocator.h)
- 레거시 핵심 아이디어:
  - `WSASend` 호출 전 `WSABUF[]`를 구성한다.
  - send completion 시점까지 조각 버퍼를 세션이 소유한다.
  - completion 이후 한 번에 정리한다.
- 현재 `RefactoringServer`에서는 아래 구조에 이 아이디어를 맞춘다.
  - [FSendBuffer.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FSendBuffer.h)
  - [FSession.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FSession.h)
  - [FSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FSession.cpp)
  - [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FIocpServer.cpp)

## 5. 개선 방향
### 5-1. 1차 목표
- `BuildPacket()`이 항상 `header + payload`를 하나의 큰 `std::vector<char>`로 다시 만드는 구조를 완화한다.
- 가능한 경우:
  - header 버퍼
  - payload 버퍼
  를 분리된 조각으로 유지한 채 전송할 수 있도록 준비한다.

### 5-2. 권장 방향
- 최종 목표는 `단일 WSASend gather 최적화`다.
- 즉:
  - `WSABUF[0] = frame header`
  - `WSABUF[1] = payload`
  형태로 한 번의 `WSASend`에 실어 보내는 구조를 검토한다.
- 이 방식이면 send in-flight는 1개를 유지하면서도, 큰 중간 버퍼 조립을 줄일 수 있다.

### 5-3. 단계적 적용
1. `BuildPacket()`의 현재 복사 비용을 계측한다.
2. `FSendBuffer`가 단일 버퍼뿐 아니라 복수 `WSABUF` 조합을 담을 수 있는지 검토한다.
3. `frame header + payload` 두 조각만으로 먼저 실어 보내는 최소 구조를 만든다.
4. 필요 시 page pool과 결합해 header 버퍼 재사용까지 연결한다.

## 6. 현재 구조에서의 적용 초안
- `FSendBuffer`는 단일 큰 payload 버퍼만 들고 있는 구조에서 시작했지만, 다음 단계에서는 조각 버퍼 묶음을 표현할 수 있어야 한다.
- 후보 방향:
  - `header` 조각
  - `payload` 조각
  - 필요 시 추가 frame 조각
- 세션은 send completion 전까지 이 조각들을 `m_activeSendBuffers` 같은 활성 send 집합에 보관한다.
- `WSASend` 완료 후에는 현재 `ReleaseActiveSendBuffers()`와 같은 지점에서 일괄 해제한다.

## 7. 검토 대상
- [FDefaultPacketFramer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FDefaultPacketFramer.cpp)
- [FSendBuffer.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FSendBuffer.h)
- [FSession.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FSession.h)
- [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\FIocpServer.cpp)

## 8. 예상 효과
- `BuildPacket()`에서 발생하는 큰 버퍼 재조립 감소
- send 경로 메모리 복사량 감소
- page pool 적용 시 payload / frame buffer 재사용 효과와 합쳐져 추가 개선 가능

## 9. 위험 요소
- `WSABUF[]` 기반으로 바꾸면 send completion 시점까지 조각 버퍼 수명 관리가 더 중요해진다.
- payload와 header를 따로 보관하면 `FSendBuffer` 책임이 커질 수 있다.
- 설계를 서두르면 현재 안정적인 send queue 규칙을 깨뜨릴 수 있으므로, 1차는 `단일 WSASend` 규칙 보존을 최우선으로 둔다.

## 10. 검증 계획
- 빌드:
  - `RefactoringServer.sln` x64 Debug
- 기능:
  - `LockFreeTests`
  - `Login -> Echo`
- 성능:
  - 기존 `Run-EchoPerfBenchmark` 시나리오로 전후 비교
  - 최소 비교 항목:
    - `recvTPS`
    - `sendTPS`
    - `recvBps`
    - `sendBps`
    - `wsaSendTPS`
    - CPU 사용량

## 11. 성공 기준
- 단일 WSASend 규칙을 깨지 않는다.
- 기능 회귀 없이 기존 Echo/Login 시나리오를 통과한다.
- 고정 작업량 벤치마크에서 복사 감소에 따른 TPS 또는 CPU 개선이 관측된다.

## 12. TODO
- 현재 Echo 고정 작업량 시나리오에서는 유의미한 TPS 개선이 관측되지 않았다.
- 후속 측정이 필요하다.
  - 더 많은 세션 수
  - 더 큰 payload
  - 더 높은 `packetsPerSend`
  - 장시간 스트레스 조건
