# RefactoringServer Project Overview

## 1. 목표
- 앞으로의 실작업 기준 루트는 `RefactoringServer/` 하나로 고정한다.
- 기존 포트폴리오 코드는 참고 자산으로만 보고, 새 서버 코어와 샘플 서버는 새 구조에서 다시 쌓는다.
- 장기 목표는 확장 가능한 MMORPG 서버지만, 현재 단계의 직접 목표는 `작게 검증 가능한 네트워크 코어`를 먼저 만드는 것이다.

## 2. 현재 프로젝트 구성
- `Foundation`
  - 공용 로깅, 진단, 설정 등 여러 모듈이 함께 쓰는 기반 계층 예정
- `NetworkLib`
  - 새 네트워크 코어 라이브러리
- `ContentsRuntime`
  - 콘텐츠 스레드, 세션 라우팅, 콘텐츠 전이 담당 프로젝트
- `Contents`
  - 서버 프로젝트가 실제 콘텐츠 구현을 두는 루트
- `Packet`
  - 콘텐츠 카테고리별 패킷 스키마 루트
- `EchoServer`
  - `NetworkLib` 최소 구동 검증용 서버
- `EchoClient`
  - echo 왕복 검증용 클라이언트
- `LockFreeTests`
  - queue, stack, memory pool 기본 병렬 검증
- `LockFreeQueueSoakTest`
  - queue 장시간 무결성 검증
- `TlsMemoryPoolSoakTest`
  - TLS memory pool 장시간 무결성 검증

## 3. 현재 설계 방향
- 네트워크 코어는 `Interlocked` 중심의 lock-free 자료구조와 비동기 I/O를 우선한다.
- 게임 로직 계층은 이후 `single-writer + message passing` 구조로 분리한다.
- 즉, 전체 시스템을 억지로 순수 lock-free로 만드는 대신, `네트워크 코어 무락 + 로직 소유권 분리`를 기본 원칙으로 둔다.
- 헤더/PCH 규칙은 [cpp-header-pch-convention.md](D:\Project\ServerPortfolio\RefactoringServer\docs\architecture\001_project\003_cpp-header-pch-convention.md)를 기준으로 맞춘다.

## 4. 현재 범위와 제외 범위
### 4-1. 현재 범위
- `IServer`, `IApplicationHandler` 기준의 최소 서버 추상화
- `FIocpServer` 기반 echo 서버 구동
- lock-free queue, stack, shared memory pool, TLS memory pool 검증

### 4-2. 아직 제외한 것
- 실제 MMORPG 도메인 모델
- 인증, 로비, 월드 분리 구조
- 패킷 프레이밍과 도메인 프로토콜 표준화
- DB 서비스, 타이머 서비스, 운영 툴 연동
- `RIO`, `boost.asio` 실제 구현

## 5. 현재 큰 축
- `NetworkLib` 코어 안정화
  - 세션, 송수신, 패킷, 성능 최적화
- `ContentsRuntime` 구조 정착
  - 콘텐츠 스레드
  - 콘텐츠 전이 규칙
  - 계측과 안정성 검증
- 콘텐츠 서버 확장
  - `AuthContent`, `EchoContent` 이후 `Lobby`, `Room` 같은 실제 흐름 추가
- 이후 `Gateway` 또는 `WorldServer` 같은 역할 분리 검토

