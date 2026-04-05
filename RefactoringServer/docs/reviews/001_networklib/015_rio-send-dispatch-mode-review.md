# RIO Send Dispatch Mode Review

## 1. 목적
- `Backend: Rio`를 유지한 채 send 경로를 `Direct`와 `OwnerThread` 두 방식으로 전환할 수 있도록 확장한 내용을 정리한다.
- 어떤 설정이 어떤 코드 경로를 타는지, 그리고 현재 비교 결과가 무엇인지 정리한다.

## 2. 설정 경로
설정 파일:
- [EchoServer.schema.yaml](D:\Project\ServerPortfolio\RefactoringServer\ConfigSchema\Server\EchoServer.schema.yaml)
- [EchoServer.yaml](D:\Project\ServerPortfolio\RefactoringServer\Config\Server\EchoServer.yaml)

설정 예:

```yaml
EchoServer:
  Backend: Rio
  RioSendDispatchMode: Direct
```

또는:

```yaml
EchoServer:
  Backend: Rio
  RioSendDispatchMode: OwnerThread
```

매핑 경로:
1. generated config loader
2. [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)
3. [BackendTypes.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\BackendTypes.h)
4. [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp)

## 3. Direct 모드
- 호출한 스레드가 바로 `RIOSend()`를 건다.
- cross-thread handoff가 없다.
- 가장 단순한 pure RIO baseline이다.

흐름:
1. app/content thread
2. `FRioServer::SendPacket(...)`
3. `SubmitSendDirect(...)`
4. `RIOSend(...)`
5. owner worker가 send completion dequeue

## 4. OwnerThread 모드
- `Send()`는 owner worker queue에 enqueue만 한다.
- 실제 `RIOSend()`는 owner worker가 수행한다.

흐름:
1. app/content thread
2. `FRioServer::SendPacket(...)`
3. `EnqueueOwnerThreadSend(...)`
4. owner worker `DrainSendCommands(...)`
5. `SubmitSendDirect(...)`
6. `RIOSend(...)`
7. owner worker가 send completion dequeue

## 5. 현재 ownership 해석
- recv completion: owner worker
- send completion: owner worker
- `Direct` 모드 send submit: 호출 스레드
- `OwnerThread` 모드 send submit: owner worker

즉:
- `Direct`는 부분 owner-thread 모델
- `OwnerThread`는 send hot path까지 owner worker로 모으는 비교 모델

## 6. 10분 파일럿 결과
| Mode | responses total | echo avg | room-change-list avg | room-change avg |
| --- | ---: | ---: | ---: | ---: |
| Rio Direct | 619436 | 3.584 ms | 8.107 ms | 9.390 ms |
| Rio OwnerThread | 664640 | 34.702 ms | 41.093 ms | 69.929 ms |
| Iocp | 669984 | 4.228 ms | 9.256 ms | 12.053 ms |

해석:
- 파일럿에서는 `OwnerThread`가 RTT에서 크게 손해를 봤다.
- 이 결과만으로는 `OwnerThread`를 기본값으로 올리기 어렵다고 판단했다.

## 7. 2시간 본실험 결과
| Mode | responses total | avg sendTPS | avg sendBps | avg CPU | echo avg | room-change-list avg | room-change avg |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Rio Direct | 1768780 | 714.084 | 456290.961 | 3.766% | 1.578 ms | 1.446 ms | 4.647 ms |
| Rio OwnerThread | 1767533 | 711.370 | 454156.445 | 3.729% | 1.470 ms | 1.459 ms | 4.778 ms |
| Iocp | 1767073 | 710.884 | 453679.689 | 3.700% | 1.464 ms | 1.418 ms | 4.651 ms |

해석:
- `Direct`가 처리량 기준으로 가장 좋았다.
- `OwnerThread`가 파일럿 때 보였던 이점은 2시간 평균에선 유지되지 않았다.
- RTT는 세 모드가 비슷하지만, `OwnerThread`가 기본값으로 갈 만큼 명확히 우세하진 않았다.

## 8. 1시간 4모드 추가 비교
추가 비교:
1. `RioDirect` (`SO_SNDBUF=0`)
2. `RioOwnerThread` (`SO_SNDBUF=0`)
3. `IocpSendBuf0`
4. `IocpSendBufDefault`

| Mode | responses total | avg sendTPS | echo avg | room-change-list avg | room-change avg |
| --- | ---: | ---: | ---: | ---: | ---: |
| Rio Direct | 884249 | 710.855 | 1.291 ms | 1.504 ms | 4.398 ms |
| Rio OwnerThread | 885203 | 715.116 | 1.195 ms | 1.519 ms | 4.363 ms |
| IocpSendBuf0 | 884536 | 712.994 | 1.228 ms | 1.634 ms | 4.941 ms |
| IocpSendBufDefault | 885876 | 713.353 | 1.116 ms | 1.444 ms | 4.488 ms |

해석:
- `RIO` 두 모드 차이는 작다.
- `IOCP`에서는 `SO_SNDBUF=-1`이 `0`보다 더 좋게 나왔다.
- 즉 `RIO`의 성능 차이는 `SO_SNDBUF`보다 send ownership 구조 영향이 더 크고, `IOCP`는 `0`을 기본값으로 강제할 이유가 약하다.

## 9. 현재 결론
- `RIO`의 기본 send 정책은 `Direct` 유지가 적절하다.
- `OwnerThread`는 후속 최적화 실험 경로로 남긴다.
- `RIO`는 현재 구현에서 `SO_SNDBUF=0` 기본으로 둬도 무방하다.
- `IOCP`는 `SO_SNDBUF=0`보다 기본값 `-1`이 더 낫거나 최소한 손해가 없다.

## 10. 1시간 4모드 재비교 (`interval=0`, `room-change=10%`)
조건:
- `250 sessions`
- `holdSeconds=3600`
- `interval=0`
- `room-count=80`
- `room-capacity=4`
- `room-change=10%`
- 서버와 클라이언트를 같은 머신에서 동시 실행

결과:
| Mode | responses total | avg sendTPS | avg CPU | echo avg | room-change-list avg | room-change avg |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Rio Direct | 20772406 | 7134.390 | 12.068% | 1.133 ms | 1.884 ms | 3.584 ms |
| Rio OwnerThread | 19731937 | 6783.761 | 11.616% | 1.541 ms | 2.490 ms | 4.261 ms |
| IocpSendBuf0 | 20398384 | 6995.189 | 12.106% | 0.927 ms | 1.673 ms | 3.187 ms |
| IocpSendBufDefault | 20591438 | 7085.586 | 12.243% | 1.089 ms | 1.826 ms | 4.033 ms |

해석:
- `Rio Direct`가 고압 조건 처리량 기준으로 가장 좋았다.
- `Rio OwnerThread`는 같은 고압 조건에서 가장 불리한 결과를 보였다.
- `IocpSendBuf0`는 지연은 좋았지만 throughput이 떨어져 기본값 후보로는 약하다.
- `IocpSendBufDefault`는 throughput이 높고 RTT도 무난해서 `IOCP` 기본값으로 더 적절하다.

주의:
- 이번 런은 서버와 클라이언트를 같은 머신에서 동시에 돌렸기 때문에 클라이언트 CPU 점유가 서버 수치에 영향을 줬을 가능성이 있다.
- 따라서 이 결과는 절대 성능이 아니라 상대 순위를 보는 baseline으로 해석하는 것이 맞다.
