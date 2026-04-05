# SendPacket Path Rewrite and Broadcast Plan

## 1. 목적
- `ContentsRuntime -> NetworkLib` send 경로의 불필요한 payload 재복사를 줄인다.
- `Broadcast` 시나리오에서 동일한 packet을 여러 세션에 fan-out할 때 header를 한 번만 만들고 재사용할 수 있게 한다.
- 이 최적화를 `NetworkLib` 내부 구현으로 숨기고, 애플리케이션 계층에는 `NetworkLib` header가 드러나지 않게 유지한다.

## 2. 핵심 원칙
1. 기본 `SendPacket` 경로 자체를 바꾼다.
2. `NetworkLib` header는 애플리케이션 계층에서 접근할 수 없어야 한다.
3. packet 공유, refcount, fan-out 같은 개념은 `NetworkLib` 내부 구현으로만 둔다.
4. 외부 API는 여전히 `SendContentPacket(...)`, 이후 `BroadcastContentPacket(...)` 같은 애플리케이션 의미 중심으로 유지한다.

## 3. 현재 문제
- 현재 `ContentsRuntime::Bridge::SendContentPacket(...)`은 content body만 serialize한 뒤 `bridge.SendRaw(sessionId, opcode, buffer, length)`를 호출한다.
- 이후 `NetworkLib::IServer::Send(...)`가 다시 `SContentHeader`를 붙이기 위해 새 `payloadBuffer`를 만들고 body를 `memcpy`한다.
- `IOCP`는 그 payload 앞에 `SPacketHeader`를 별도 `WSABUF`로 붙인다.
- `RIO`는 `BuildPacket(...)`으로 transport packet을 다시 하나의 연속 버퍼로 조립한다.

즉 현재 구조에서는:
- content body serialize
- `SContentHeader + body` 재복사
- 경우에 따라 `SPacketHeader + payload` 재조립

이 순서가 생긴다.

## 4. 레거시 참고점
- 레거시 `CSerializableBuffer`는 생성 시 앞쪽에 `NetworkLib` header 영역을 확보한 뒤 body를 뒤쪽에 썼다.
- `SendPacket()` / `EnqueuePacket()`는 `m_isEnqueueHeader`를 보고 header를 한 번만 채웠다.
- 같은 버퍼 포인터를 여러 세션 send queue에 넣고, `IncreaseRef()` / `DecreaseRef()`로 수명을 관리했다.

즉 레거시에는 이미 다음 특성이 있었다.
- front headroom 보유
- header 1회 작성
- immutable packet fan-out
- refcount 기반 broadcast 공유

## 5. 전제
- 현재 프로젝트에서는 `세션별 암호화 키`를 사용할 계획이 없다.
- 따라서 같은 logical packet은 세션 간에 같은 transport payload / checksum / randomKey를 공유할 수 있다고 가정한다.
- 나중에 세션별 암호화 정책이 생기면 broadcast packet 공유는 다시 제한하거나 분기해야 한다.

## 6. 방향
### 6.1 공개 API
- 애플리케이션 계층은 여전히 body serialize만 담당한다.
- 애플리케이션은 `SContentHeader`, `SPacketHeader`, checksum, randomKey, front headroom 크기를 몰라야 한다.
- 공개 API는 다음 성격을 유지한다.
  - `SendContentPacket(...)`
  - `BroadcastContentPacket(...)`

### 6.2 내부 구현
- `NetworkLib` 내부에 send packet storage를 새로 둔다.
- 이 storage는 다음을 가진다.
  - `NetworkLib`용 front headroom
  - serialized body
  - immutable finalize 상태
  - shared ownership 또는 refcount
- 애플리케이션은 opaque writer / opaque outgoing packet만 다루고, 내부 레이아웃은 볼 수 없다.

### 6.3 핵심 변화
- 지금의 `SendRaw(sessionId, opcode, const char* buffer, length)`는 결국 내부에서 새 payload를 다시 만들게 만든다.
- 이를 `owned outgoing packet`을 전달하는 방식으로 바꾸거나 확장해야 한다.
- 즉 기본 send path의 소유권 모델을 바꾸는 것이 핵심이다.

## 7. 제안 구조
### 7.1 Opaque outgoing packet
- `NetworkLib` 내부에 `send packet storage`를 둔다.
- 외부에는 packet 내부 레이아웃이 아니라 다음 정도만 보인다.
  - body writer 획득
  - serialize 완료
  - send 또는 broadcast 요청

예시 개념:
- `CreateOutgoingContentPacket(opcode, estimatedBodySize)`
- `GetBodyWriter()`
- `SendOutgoingPacket(sessionId, packet)`
- `BroadcastOutgoingPacket(targets, packet)`

여기서도 `NetworkLib` header는 외부로 노출하지 않는다.

### 7.2 body serialize 방식
- writer는 내부적으로 front headroom을 가진 버퍼를 쓴다.
- 애플리케이션은 body만 serialize한다.
- serialize 완료 후 `NetworkLib`가 자기 header를 앞쪽에 기록한다.
- 즉 body 재복사는 없애고, header만 finalize 단계에서 채운다.

### 7.3 Broadcast
- `Broadcast`는 동일한 immutable packet storage를 여러 세션 queue에 fan-out한다.
- 각 세션은 packet 자체를 복사하지 않고 소유권만 증가시킨다.
- `IOCP`
  - 여러 세션이 같은 immutable payload를 가리키는 `WSABUF` 구성이 가능해야 한다.
- `RIO`
  - 여러 세션에서 같은 packet을 재사용하더라도 registered buffer 정책은 별도 설계가 필요하다.
  - 1차는 storage 공유만 하고, `RIORegisterBuffer`는 submit 시점에 개별 등록해도 된다.

## 8. 계층별 변경 방향
### 8.1 ContentsRuntime / Bridge
- 현재:
  - `SendRaw(sessionId, opcode, const char* buffer, length)`
- 목표:
  - owned outgoing packet을 넘길 수 있는 경로로 확장
  - 단, 외부 타입은 `NetworkLib` header를 직접 노출하지 않는 opaque 성격이어야 한다

### 8.2 Packet Serialization
- 현재:
  - body serialize 후 별도 header 결합
- 목표:
  - body writer가 내부 front headroom을 가진 버퍼에 직접 쓰게 한다
  - `BuildContentPayload(...)` 같은 재복사 helper는 축소 또는 제거한다

### 8.3 NetworkLib send path
- `IOCP`
  - 내부 packet storage 앞쪽에 `SContentHeader`를 직접 기록
  - `SPacketHeader`는 1차에서 기존처럼 별도 `WSABUF` 유지 가능
- `RIO`
  - 내부 packet storage 앞쪽에 `SContentHeader`를 직접 기록
  - 이후 `SPacketHeader`까지 같은 buffer 안에 넣을지, builder를 유지할지는 별도 측정 후 판단

## 9. 단계별 구현 순서
### 1단계
- 기본 `SendPacket` 경로를 새 소유권 모델로 교체
- body 재복사 제거
- 단일 send 경로를 새 outgoing packet 모델로 연결

### 2단계
- `BroadcastContentPacket(...)` 추가
- 하나의 immutable packet을 여러 세션에 fan-out
- `IOCP`에서 수명 관리 검증

### 3단계
- `RIO` send path를 같은 outgoing packet 모델 기준으로 정리
- registered buffer 비용과 공유 정책 측정

### 4단계
- 필요하면 `SPacketHeader`까지 headroom화해서 최종 transport copy를 추가로 줄인다

## 10. 주의사항
- send queue에 들어간 뒤 packet은 immutable이어야 한다.
- finalize 이후에는 payload/body를 수정하면 안 된다.
- 동일 packet fan-out은 checksum/randomKey가 세션별로 달라지지 않는다는 전제가 있어야 한다.
- disconnect 중인 세션에 enqueue 실패하면 refcount 해제가 누락되지 않도록 해야 한다.
- `IOCP` active send batch와 `RIO` completion callback이 같은 packet storage를 참조할 수 있으므로 수명 종료 시점을 명확히 해야 한다.

## 11. 검증 계획
### 기능 검증
- 단일 send가 기존과 동일하게 동작하는지
- `Broadcast`가 N 세션에 모두 동일 payload를 전달하는지
- disconnect / partial failure에서도 leak 없이 정리되는지

### 성능 검증
- `IOCP`
  - 기존 대비 `sendTPS`, CPU%, queued send buffers 비교
- `RIO`
  - 기존 baseline 대비 `sendTPS`, RTT avg, buffer register cost 비교
- `Broadcast`
  - N 세션 fan-out 시 packet 생성 횟수와 memcpy 횟수 감소 확인

## 12. 기대 효과
- `Contents body -> NetworkLib payload` 재복사 제거
- `Broadcast` 시 header 1회 작성 + payload 공유 가능
- `IOCP` / `RIO` send path를 공통 상위 모델로 정리 가능
- `NetworkLib` header를 애플리케이션 계층에서 완전히 숨긴 채 최적화 가능

## 13. 후속 메모
- `SO_SNDBUF=0` 옵션은 커널 송신 버퍼 정책 조절일 뿐, 위 구조 최적화와는 별개다.
- 진짜 send copy 감소는 기본 `SendPacket` 경로 교체와 broadcast fan-out 구조 쪽이 핵심이다.
