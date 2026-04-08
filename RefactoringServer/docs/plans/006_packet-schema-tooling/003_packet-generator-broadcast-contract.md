# PacketGenerator Broadcast Contract Plan

## 1. 목적
- `PacketGenerator`가 `broadcast` 패킷을 스키마에서 직접 표현하고 생성할 수 있게 만든다.
- 현재 `rq / rp / noti`만으로는 `room fan-out` 같은 server-originated broadcast intent를 문서와 코드에서 분명하게 드러내기 어렵다.
- 채팅 서버 같은 다중 수신자 시나리오를 위해 `broadcast`를 별도 계약으로 정리한다.

## 2. 배경
- 현재 schema는 대체로 다음 구조다.
  - `rq`
  - `rp`
  - `noti`
- wire format 관점에서 `broadcast`도 결국 one-way packet이지만, 의미상으로는 다음 차이가 있다.
  - 단일 세션 대상 알림이 아니라 다수 세션 fan-out을 전제
  - roomId, senderId, sequence 같은 broadcast 메타를 자주 동반
  - 이후 `NetworkLib` broadcast helper 또는 shared payload 전략과 연결될 가능성이 큼
- 따라서 `noti`에 모두 우겨 넣기보다 schema에서 intent를 분리하는 편이 좋다.

## 3. 목표
- schema에 `broadcast` 블록을 추가한다.
- generator가 `broadcast`도 `rq / rp / noti`와 같은 수준의 packet type으로 생성한다.
- router / handler / dispatcher가 `broadcast`를 정상적으로 처리한다.
- 기존 `noti`는 유지한다.

## 4. 범위
대상:
- [Program.cs](D:\Project\ServerPortfolio\RefactoringServer\Tools\PacketGenerator\Program.cs)
- schema parser / validator
- generated packet/handler/router contract
- `Chat.yaml` 같은 실제 콘텐츠 스키마

비범위:
- transport-layer broadcast fan-out 구현
- shared payload refcount 구현
- `NetworkLib` send API 변경

## 5. 스키마 제안
### 5-1. 기본 형태
```yaml
content: Chat

messages:
  - name: ChatSend
    rq:
      opcode: 3010
      fields:
        - { name: roomId, type: uint32 }
        - { name: payload, type: bytes, maxEncodedSizeBytes: 8192 }
    broadcast:
      opcode: 3011
      fields:
        - { name: roomId, type: uint32 }
        - { name: senderUserId, type: uint64 }
        - { name: messageId, type: uint64 }
        - { name: payload, type: bytes, maxEncodedSizeBytes: 8192 }
```

### 5-2. 의미
- `broadcast`는 one-way packet이다.
- `rq/rp` pair 규칙에 묶지 않는다.
- `noti`와 wire-level 차이는 없지만, schema intent와 generated 이름을 분리한다.

## 6. 생성 규칙
### 6-1. packet class
- `broadcast`도 일반 packet class를 생성한다.
- 예:
  - `FChatSendBroadcast`
  - 또는 naming rule에 맞는 동등 클래스

### 6-2. handler / dispatcher
- generator는 `broadcast`용 handler entry도 만든다.
- 즉 수신 측에서 `broadcast` packet을 다른 one-way packet처럼 dispatch할 수 있어야 한다.

### 6-3. router
- opcode table에 `broadcast` opcode를 포함한다.
- `PacketRouter`는 다른 packet과 동일하게 `broadcast`도 route한다.

## 7. validation 규칙
### 7-1. 허용
- 한 message 안에 다음 조합을 허용한다.
  - `rq + rp`
  - `rq + rp + noti`
  - `rq + rp + broadcast`
  - `broadcast` only
  - `noti` only

### 7-2. 금지 또는 주의
- 같은 message 안에서 `noti`와 `broadcast`를 둘 다 둘지는 1차에 금지하는 편이 단순하다.
- `rq`만 있고 `rp`가 없는 기존 금지 규칙은 유지한다.
- opcode 중복 금지는 그대로 유지한다.

### 7-3. size 규칙
- `broadcast`는 큰 payload에 자주 쓰일 가능성이 크므로, `string`, `bytes`, `vector<T>` 같은 가변 길이 필드가 있으면 `maxEncodedSizeBytes`를 강제하는 쪽이 좋다.
- 이 규칙은 기존 `PacketGenerator` size 정책과 맞춘다.

## 8. generated API 방향
### 8-1. 현재 단계
- generator는 packet type, serialize/deserialize, handler, dispatcher만 생성한다.
- `broadcast`라고 해서 transport helper를 자동 생성하지는 않는다.

### 8-2. 후속 확장 가능성
- 이후 `BroadcastContentPacket(...)` 같은 helper가 들어오면 generator가 `broadcast` packet을 더 잘 연결할 수 있다.
- 하지만 1차는 schema intent를 드러내는 것만으로 충분하다.

## 9. 구현 순서
1. schema parser에 `broadcast` 노드 추가
2. validator에 `broadcast` 규칙 추가
3. codegen template에 packet/handler/dispatcher generation 추가
4. `Chat.yaml`에 `ChatBroadcast` 적용
5. generator 재실행
6. generated build 확인
7. 실제 server/client 샘플에서 route/dispatch 확인

## 10. 검증 계획
### 10-1. tooling
- `broadcast`가 포함된 yaml에서 generation 성공
- opcode 중복, 잘못된 조합에서 generation 실패
- 가변 길이 payload에 최대 크기 미기재 시 generation 실패

### 10-2. build
- generated packet/handler/router 빌드 성공

### 10-3. runtime
- `ChatBroadcast` packet serialize / deserialize round-trip
- room fan-out 시 각 클라이언트가 정상 수신

## 11. 기대 효과
- schema만 봐도 “이 packet은 room fan-out용”이라는 intent가 드러난다.
- 채팅 서버/대형 payload 테스트에서 packet 역할이 더 분명해진다.
- 이후 `NetworkLib` broadcast 경로 최적화와 자연스럽게 연결할 수 있다.
