# ClientNetworkLib 및 ChattingDummyClient 계획

## 1. 목적
- `ChattingServer`의 큰 패킷 전송 성능 비교를 위해, 더미 클라이언트를 바로 구현하기보다 먼저 범용 클라이언트 네트워크 라이브러리 `ClientNetworkLib`를 만든다.
- `ClientNetworkLib` 위에 1차 벤치마크용 애플리케이션 `ChattingDummyClient`를 구현한다.
- 이후 다른 더미 테스트 클라이언트가 필요해져도 동일한 네트워크 계층을 재사용할 수 있게 한다.

## 2. 핵심 결정
- 1차 우선순위는 `ChattingDummyClient` 자체가 아니라 `ClientNetworkLib`다.
- `ClientNetworkLib`는 `IOCP` 기반으로 구현한다.
- 세션당 전용 스레드는 절대 사용하지 않는다.
- `IOCP worker thread`는 고정 개수만 사용한다.
  - 기본 목표값: `4`
  - 의미: 네트워크 처리용 worker thread 수가 4개라는 뜻이며, 메인/제어 스레드는 별도로 존재할 수 있다.
- 내부 구현은 락프리보다 안정성과 추적 가능성을 우선한다.
  - `mutex`
  - `condition_variable`
  - 명확한 소유권
  - 방어적인 lifetime 관리
- `ChattingDummyClient`는 `ClientNetworkLib`를 사용하는 첫 번째 소비자 애플리케이션이다.

## 3. 왜 이렇게 가는가
- 지금 목표는 `ChattingServer`의 `RIO / IOCP` 비교이지, 클라이언트 구현을 임시로 한 번 쓰고 버리는 것이 아니다.
- 더미 클라이언트를 바로 만들면 이후 다른 성능 테스트나 프로토콜 테스트에서 네트워크 코드를 또 복사하게 될 가능성이 높다.
- `EchoClient`는 참고용으로 활용하되, 최종 구조는 `ClientNetworkLib + ChattingDummyClient` 2계층으로 분리하는 편이 장기적으로 낫다.
- `select` 기반 싱글 스레드 클라이언트는 1차 구현이 단순하다는 장점은 있으나, 세션 수가 늘어나면 클라이언트가 먼저 병목이 될 수 있다.
- 반면 `IOCP` 기반 고정 워커 모델은 세션당 스레드 없이도 확장성이 좋고, 이후 재사용 라이브러리로 발전시키기 쉽다.

## 4. 전체 구조
### 4.1 계층
- `ClientNetworkLib`
  - 범용 네트워크 계층
  - connect / disconnect / reconnect
  - send queue / recv 처리
  - packet framing / cipher
  - event delivery
  - IOCP worker 관리
- `ChattingDummyClient`
  - `ChattingServer` 전용 시나리오 계층
  - 로그인
  - room 목록 수신
  - room 선택
  - room 변경
  - 채팅 전송
  - broadcast 검증
  - RTT/통계 수집

### 4.2 향후 확장
- 이후 다른 더미 테스트 클라이언트가 필요하면 `ClientNetworkLib`를 그대로 재사용한다.
- 추후 `ChattingClient` WinForms 버전도 필요하면 네트워크 부분은 같은 라이브러리 또는 같은 설계 원칙을 공유하도록 유도한다.

## 5. ClientNetworkLib 범위
### 포함
- Windows `IOCP` 기반 비동기 소켓 처리
- 고정 개수 worker thread
- connect / disconnect / graceful close
- 송신 큐
- 수신 버퍼 누적 및 packet frame 파싱
- packet cipher / packet framer 재사용
- 세션 상태 추적
- 상위 계층으로 이벤트 전달

### 제외
- 락프리 큐
- 세션당 전용 스레드
- 게임/채팅 도메인 로직
- room 상태 머신
- RTT 통계 정책 자체

## 6. ClientNetworkLib 설계 원칙
### 6.1 안정성 우선
- 락프리보다 안전성과 디버깅 편의성을 우선한다.
- ownership은 최대한 단순하게 가져간다.
- 종료 시퀀스, disconnect 경계, in-flight I/O 정리를 명시적으로 관리한다.

### 6.2 스레드 모델
- 세션당 스레드 금지
- 네트워크 worker thread는 고정 개수만 사용
- 기본 목표 구성:
  - `main/control thread` 1개
  - `IOCP worker thread` 4개
- 상위 애플리케이션은 이 worker 수를 설정으로 조정할 수 있게 하되, 초기 기본값은 `4`로 둔다.

### 6.3 이벤트 모델
- 상위 애플리케이션은 네트워크 라이브러리로부터 다음 이벤트를 받는다.
  - connected
  - connect failed
  - disconnected
  - packet received
  - send failed
  - fatal session error
- 이벤트 전달 방식은 1차 구현에서 아래 둘 중 하나를 택한다.
  - thread-safe event queue + `PollEvents()`
  - interface callback
- 1차 목표는 단순성과 안전성이므로 `event queue + PollEvents()` 쪽이 더 적합하다.

### 6.4 send 정책
- 상위 계층은 `SendPacket(sessionId, packet)` 수준 API를 사용한다.
- 실제 `WSASend` 호출과 in-flight 관리, partial send 대응은 라이브러리 내부에서 숨긴다.
- 패킷 단위 송신만 노출하고, 상위 계층이 소켓 write 상태를 직접 다루지 않게 한다.

## 7. ChattingDummyClient 범위
### 포함
- `LoginRq/Rp`
- `RoomListRq/Rp`
- 초기 room 입장용 `RoomChangeRq/Rp`
- in-room `ChattingRq/Rp`
- `Broadcast` 수신 및 검증
- 다중 세션 부하 생성
- RTT / throughput 측정
- 콘솔 / CSV 출력

### 제외
- GUI
- 운영 도구
- 채팅 UI
- 메시지 영속화

## 8. ChattingDummyClient 상태 머신
각 세션은 아래 흐름을 따른다.

1. connect
2. `LoginRq/Rp`
3. `RoomListRq/Rp`
4. room 선택
5. `RoomChangeRq/Rp`로 `NoRoom -> Room`
6. steady state
   - `ChattingRq` 전송
   - `ChattingRp` 수신 대기
   - 다른 세션의 `Broadcast` 수신 및 검증
   - 필요 시 `RoomChangeRq`
7. 설정에 따라 유지 / 재접속 / 종료

1차 규칙:
- 세션당 동시에 outstanding 상태인 `ChattingRq`는 하나만 둔다.
- RTT 기준은 `ChattingRq -> ChattingRp`
- `Broadcast`는 fan-out throughput 및 검증 지표로 별도 관리
- self-broadcast는 실패로 간주

## 9. room 선택 모드
- `Random`
  - 후보 room 중 무작위 선택
- `RoundRobin`
  - room을 순서대로 순환 선택
- `Hotspot`
  - 일부 room에 세션을 의도적으로 집중시켜 fan-out 압력을 만드는 방식

## 10. 워크로드 모델
### 연결
- `SessionCount`
- `ConnectsPerSecond`
- `ReconnectProbabilityPercent`
- `ReconnectDelayMs`

### 채팅 전송
- `SendIntervalMs` 기반 주기 전송
- 세션별로 다음 전송 시각을 상태로 관리
- 세션당 별도 스레드를 두지 않고, 상위 루프 또는 스케줄러가 전송 가능 세션을 골라 send 요청을 넣는다

### payload
- `1 KiB`
- `2 KiB`
- `4 KiB`
- `8 KiB` 근접 크기
- 향후 mixed profile 확장 가능하도록 설계

## 11. 검증 규칙
- `LoginRp.success`가 true여야 다음 단계로 이동
- `RoomChangeRp.success`가 true여야 채팅 시작
- `ChattingRp.success=false`는 성공 트래픽으로 집계하지 않음
- `Broadcast.roomId`는 현재 room과 일치해야 함
- `Broadcast.senderUserId`는 자기 자신의 user id와 같으면 안 됨
- `Broadcast.payload.size()`는 기대 크기와 일치해야 함
- payload 검증이 켜져 있으면 checksum 또는 패턴이 일치해야 함
- room 변경 이후 이전 room broadcast는 더 이상 수신되면 안 됨

## 12. 측정 지표
### 처리량
- connect TPS
- login TPS
- room change TPS
- `ChattingRq` send TPS
- `ChattingRp(success=true)` TPS
- broadcast receive TPS
- send bytes/sec
- receive bytes/sec

### 지연시간
- `ChattingRq -> ChattingRp` RTT
- 선택적 단계별 RTT
  - login
  - room list
  - room change

### 실패/검증
- login 실패 수
- room change 실패 수
- chatting reject 수
- self-broadcast 수
- invalid-room broadcast 수
- payload 검증 실패 수
- timeout 수
- reconnect 수

### 출력
- 주기적 콘솔 요약
- RTT CSV
- summary CSV 또는 로그 파일

## 13. Generator 및 설정 작업
### 13.1 PacketGenerator
- `ClientNetworkLib` 자체를 위해 packet-generator 수정은 필요 없다.
- `ChattingDummyClient`는 아래 generated packet을 재사용한다.
  - `Generated/Packets/Chatting/ChattingPackets.h`
  - `Generated/Packets/Login/LoginPackets.h`

### 13.2 ConfigGenerator
우선 `ChattingDummyClient`용 설정 스키마부터 추가한다.
- `ConfigSchema/Client/ChattingDummy.schema.yaml`
- `Generated/Config/ChattingDummy/ChattingDummyConfig.h`
- `Generated/Config/ChattingDummy/ChattingDummyConfig.cpp`
- `Config/Client/ChattingDummy.yaml`

추후 필요하면 `ClientNetworkLib` 공통 설정과 `ChattingDummyClient` 전용 설정을 분리한다.
1차 구현에서는 아래 `14. 1차 Config 확정안`에 적은 항목만 실제 schema에 포함한다.

## 14. 1차 Config 확정안
이번 1차 구현에서는 아래 항목만 `ChattingDummyClient` 설정으로 사용한다.

### 공통 네트워크
- `ServerIp`
- `Port`
- `WorkerThreadCount`

### 세션/워크로드
- `SessionCount`
- `ConnectsPerSecond`
- `LoginUserIdBase`
- `RunSeconds`
- `SendIntervalMs`
- `PayloadSizeBytes`
- `RoomSelectionMode`
- `HotspotRoomIds`
- `HotspotBiasPercent`
- `RoomChangeProbabilityPercent`
- `ReconnectProbabilityPercent`
- `ReconnectDelayMs`

### 검증/출력
- `ResponseTimeoutMs`
- `ConsoleSummaryIntervalSeconds`
- `RttCsvPath`

### 1차에서는 설정으로 빼지 않는 항목
- `DisableNagle = true`
- `ValidatePacketChecksum = true`
- `ValidateNoSelfBroadcast = true`
- `ValidateBroadcastRoomId = true`
- `MaxOutstandingChatPerSession = 1`

## 15. 프로젝트 구조
### 신규 프로젝트
- `RefactoringServer/ClientNetworkLib`
- `RefactoringServer/ChattingDummyClient`

### 1차 ClientNetworkLib 파일 예시
- `Pch.h`
- `Pch.cpp`
- `ClientNetworkLib.vcxproj`
- `Public/...`
- `Private/...`

### 1차 ChattingDummyClient 파일 예시
- `Pch.h`
- `Pch.cpp`
- `Main.cpp`
- `ChattingDummyClient.vcxproj`

## 16. 구현 순서
1. 이 문서 기준으로 `ClientNetworkLib -> ChattingDummyClient` 우선순위를 고정한다.
2. `ClientNetworkLib` 프로젝트를 스캐폴드한다.
3. `IOCP` 기반 connect / recv / send / disconnect 기본 흐름을 구현한다.
4. thread-safe event queue 기반 상위 이벤트 전달 구조를 붙인다.
5. 소규모 echo 수준 smoke test로 `ClientNetworkLib` 자체를 먼저 검증한다.
6. `ChattingDummyClient` config schema와 generated config를 추가한다.
7. `ChattingDummyClient` 프로젝트를 만들고 `ClientNetworkLib`를 연결한다.
8. `Login -> RoomList -> RoomChange -> Chatting` 흐름을 구현한다.
9. `Broadcast` 수신 및 검증을 추가한다.
10. RTT CSV와 summary 출력까지 붙인다.
11. 소규모 smoke test 후 큰 payload benchmark로 확장한다.

## 17. 테스트 계획
### ClientNetworkLib 자체
- connect / disconnect 반복
- 다중 세션 송수신
- 강제 disconnect 처리
- send queue drain 및 종료 시퀀스 검증

### ChattingDummyClient smoke
- `1 session`
- `2 sessions` same room
- `2 sessions` different room
- 초기 입장 후 room change

### 기능 검증
- self-broadcast가 없는지 확인
- room 변경 후 이전 room broadcast를 받지 않는지 확인
- `8 KiB` 근접 payload 송수신이 정상 동작하는지 확인

### 부하 검증
- 점진적 램프업
- hotspot room 시나리오
- mixed room 시나리오
- 장시간 유지 시나리오

## 18. 후속 작업
- `ChattingDummyClient`가 안정화되면, 별도 `ChattingClient` WinForms 계획 문서를 만든다.
- 이후 다른 더미 테스트 클라이언트가 필요해지면 `ClientNetworkLib`를 공통 기반으로 재사용한다.
