# 버퍼 재사용 및 Page Pool 계획

## 1. 목적
- `NetworkLib` 핵심 경로에서 반복적으로 발생하는 바이트 버퍼 할당을 줄인다.
- TLS 객체 풀과 별도로 바이트 버퍼 재사용 전략을 정리한다.
- 필요할 때만 page 성격의 재사용을 켜고 끌 수 있도록 옵션 기반 구조로 만든다.

## 2. 현재 상태
- 현재 TLS 풀이 적용된 대상:
  - `FSession`
  - `FSendBuffer`
  - `FPacketWriter` 내부의 `FPacketBuffer`
- 아직도 성능 영향이 큰 버퍼 경로:
  - `FIocpServer::Send()`의 framed payload 조립
  - `FDefaultPacketFramer::BuildPacket`
  - 샘플 클라이언트의 packet batch buffer

## 3. 1차 목표
- `NetworkLib` 코어 경로의 임시 `vector<char>` 생성 횟수를 줄인다.
- page 성격의 capacity 재사용을 옵션으로 제공한다.
- 객체 재사용과 바이트 버퍼 재사용 책임을 분리한다.

## 4. 방향
### 4-1. 객체 재사용
- 세션, 송신 버퍼, 직렬화 writer 같은 객체는 TLS 풀을 계속 사용한다.

### 4-2. 바이트 버퍼 재사용
- 바이트 버퍼는 page 단위 capacity 재사용을 선택적으로 사용한다.
- 우선 후보:
  - send payload buffer
  - framed wire packet buffer
  - packet batch buffer

### 4-3. Page Pool 역할
- page pool은 고정 크기 page를 재사용하는 개념으로 본다.
- 기본 page 크기 후보:
  - `4KB`
  - 필요 시 `8KB`
- packet 크기가 page보다 작으면 page 내부 capacity 재사용으로 대응한다.
- packet 크기가 더 크면 일반 fallback 경로를 사용한다.

## 5. 옵션 정책
### 5-1. 기본값
- `enablePageBufferReuse = true`
- `pageBufferSize = 4096`

### 5-2. 서버 경로
- `SServerConfig.enablePageBufferReuse`
- `SServerConfig.pageBufferSize`

### 5-3. 샘플 클라이언트 경로
- `--disable-page-pool`
- `--page-size <bytes>`

### 5-4. 비활성화 시 동작
- wrapper 객체 자체는 TLS 풀로 재사용한다.
- 내부 `std::vector<char>` capacity는 reset 시 비워서 page 성격 재사용이 일어나지 않도록 한다.

## 6. 적용 후보 위치
- `Packet/FPacketWriter`
- `Packet/FPacketSerialization`
- `Packet/FDefaultPacketFramer`
- 필요 시 `Servers/FSendBuffer`

## 7. 단계별 적용
1. 현재 버퍼 할당 위치를 계측한다.
2. `FPacketWriter`에 page reuse 옵션을 연결한다.
3. `BuildPacket`과 send frame 조립 경로에 같은 정책을 연결한다.
4. TPS, CPU, bytes/sec 기준으로 전후를 비교한다.

## 8. 주의점
- lock-free container 내부 메모리 관리 코드는 이 범위에서 건드리지 않는다.
- page pool은 바이트 버퍼 최적화이지, 객체 TLS 풀과 동일한 개념이 아니다.
- 1차 단계에서는 과도한 page chain 구조까지 열지 않는다.

## 9. 성공 기준
- 평균 TPS 상승 또는 같은 TPS에서 CPU 사용량 감소
- `wsaSendTPS`, `wsaRecvTPS`와 함께 해석 가능한 수준의 개선
- `Login -> Echo`와 스트레스 시나리오 모두 기능 유지
