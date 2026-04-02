# Buffer Reuse And Page Pool Plan

## Page Reuse Toggle
- Page-sized buffer reuse must be configurable, not hard-wired.
- Default policy:
  - `enablePageBufferReuse = true`
  - `pageBufferSize = 4096`
- Server path uses `SServerConfig.enablePageBufferReuse` and `SServerConfig.pageBufferSize`.
- Sample client path uses runtime options:
  - `--disable-page-pool`
  - `--page-size <bytes>`
- When disabled, pooled wrapper objects remain pooled, but the internal `std::vector<char>` capacity is released on reset so page-style reuse is effectively off.

## 1. 목적
- `NetworkLib` 송수신 경로에서 반복적으로 생기는 바이트 버퍼 할당을 줄인다.
- TLS 객체 풀과 별도로, 바이트 버퍼 자체를 재사용하는 `page pool` 전략을 도입할지 기준을 세운다.

## 2. 현재 상태
- 현재 TLS 풀 적용 대상:
  - `FSession`
  - `FSendBuffer`
  - `FPacketWriter` 내부 `FPacketBuffer`
- 아직 `std::vector<char>` 재할당이 남아 있는 경로:
  - `FIocpServer::Send()`의 framed payload 조립
  - `FDefaultPacketFramer::BuildPacket`
  - 테스트/샘플 클라이언트의 packet batch buffer

## 3. 1차 목표
- `NetworkLib` 코어 경로에서 임시 payload/vector 생성 횟수를 줄인다.
- `page pool` 도입 전후의 기준선을 잡는다.
- 객체 풀과 바이트 풀의 책임을 분리한다.

## 4. 방향

### 4.1 객체 재사용
- 세션, 송신 버퍼, 패킷 writer 같은 객체는 TLS 풀 재사용을 유지한다.

### 4.2 바이트 버퍼 재사용
- 바이트 버퍼는 `page pool` 후보로 본다.
- 우선 후보:
  - send payload page
  - framed wire packet page
  - packet batch page

### 4.3 page pool 역할
- `page pool`은 고정 크기 페이지를 재사용한다.
- 추천 기본 page size:
  - 4KB 또는 8KB
- packet 크기가 page보다 작으면 page 내부 일부만 사용한다.
- packet 크기가 page보다 크면:
  - 여러 page를 체인으로 묶거나
  - fallback heap/vector 경로를 둔다.

## 5. 도입 후보 위치
- `Packet/FPacketWriter`
- `Packet/FPacketSerialization`
- `Packet/FDefaultPacketFramer`
- 필요 시 `Servers/FSendBuffer`

## 6. 단계별 적용
1. 현재 버퍼 할당 위치 계측
2. `FPacketWriter` page-backed 옵션 검토
3. `BuildPacket` / send frame 조립 경로 page pool 후보화
4. 실제 TPS / CPU / 할당량 비교

## 7. 주의점
- lock-free container 내부 메모리 관리 코드는 이번 범위에서 건드리지 않는다.
- `page pool`은 바이트 버퍼용이고, 객체용 TLS 풀과 섞지 않는다.
- 너무 이른 다중 page 체인 지원은 복잡도를 크게 올리므로 1차는 단일 page 중심이 낫다.

## 8. 성공 기준
- 평균 TPS 상승 또는 같은 TPS에서 CPU 사용량 감소
- `wsaSendTPS`, `wsaRecvTPS` 변화와 함께 해석 가능
- 기능 회귀 없이 `Login -> Echo` 및 스트레스 시나리오 유지
