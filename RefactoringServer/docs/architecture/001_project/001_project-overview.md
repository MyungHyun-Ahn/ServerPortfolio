# RefactoringServer Project Overview

## 1. 목표
- 앞으로의 실작업 기준 루트는 `RefactoringServer/` 하나로 고정한다.
- 기존 포트폴리오 코드는 참고 자산으로만 보고, 새 서버 코어와 샘플 서버는 새 구조에서 다시 쌓는다.
- 장기 목표는 확장 가능한 MMORPG 서버지만, 현재 단계의 직접 목표는 `작게 검증 가능한 네트워크 코어 + 인증/채팅 수직 흐름`을 먼저 만드는 것이다.

## 2. 현재 프로젝트 구성
- `Foundation`
  - 공용 로깅, 진단, 설정 등 여러 모듈이 함께 쓰는 기반 계층
- `NetworkLib`
  - 새 네트워크 코어 라이브러리
- `ContentsRuntime`
  - 콘텐츠 스레드, 세션 라우팅, 콘텐츠 전이를 담당하는 런타임 계층
- `Connector`
  - 외부 인증 저장소와 런타임 사이를 잇는 인프라 계층
- `Contents`
  - 서버 프로젝트가 실제 콘텐츠 구현을 두는 루트
- `Packet`
  - 콘텐츠 카테고리별 패킷 스키마 루트
- `Echo`
  - 네트워크 코어 최소 구동 검증용 샘플 서버/클라이언트
- `Chatting`
  - `ChattingServer`, `ChattingDummyClient`, `ChattingClientWinForms`를 포함한 채팅 검증 루트
- `LoginServer`
  - `Node.js + TypeScript + Express` 기반 외부 인증 서버
- `Infra`
  - `MySQL + Redis + LoginServer` Docker 기동과 실행 스크립트 루트
- `SmokeTests`
  - queue, stack, memory pool, soak test 등 기본 검증 루트
- `Tools`
  - `PacketGenerator`, `ConfigGenerator` 같은 생성 도구 루트

## 3. 현재 설계 방향
- 네트워크 코어는 `Interlocked` 중심의 lock-free 자료구조와 비동기 I/O를 우선한다.
- 게임 로직 계층은 `single-writer + message passing` 구조로 분리한다.
- 전체 시스템을 억지로 순수 lock-free로 만드는 대신, `네트워크 코어 무락 + 로직 소유권 분리`를 기본 원칙으로 둔다.
- 실시간 채팅 세션과 room 상태는 `ChattingServer`가 소유하고, 계정 인증과 비밀번호 검증은 외부 `LoginServer`로 분리한다.
- 외부 인증에서 `ChattingServer`로 넘어갈 때는 `Redis chat ticket`과 `LoginAuth` 패킷을 경계로 사용한다.
- 헤더/PCH 규칙은 [003_cpp-header-pch-convention.md](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/docs/architecture/001_project/003_cpp-header-pch-convention.md)를 기준으로 맞춘다.

## 4. 현재 범위와 제외 범위

### 4-1. 현재 범위
- `IServer`, `IApplicationHandler` 기준의 최소 서버 추상화
- `FIocpServer`, `FRioServer` 기반 네트워크 코어 검증
- lock-free queue, stack, shared memory pool, TLS memory pool 검증
- `ChattingServer`의 `Lobby -> Room -> Chatting -> Broadcast` 흐름
- `Node.js LoginServer + MySQL + Redis` 기반 외부 인증과 채팅 입장 ticket 발급
- `ChattingServer LoginAuth` 기반 인증 연동과 중복 로그인 강제 교체
- `WinForms` 기반 로그인/회원가입/방 입장/채팅 수동 검증 경로
- `PowerShell + YAML manifest` 기반 `BenchmarkRunner`

### 4-2. 아직 제외한 것
- 실제 MMORPG 도메인 모델
- 월드 분리 구조와 다중 서버 샤딩
- HTTPS, rate limiting, 운영용 인증 보안 강화
- DB 서비스, 타이머 서비스, 운영 툴 연동 확장
- 운영용 계정/권한 체계 고도화

## 5. 현재 큰 축
- `NetworkLib` 코어 안정화
  - 세션, 송수신, 패킷, 성능 최적화
- `ContentsRuntime` 구조 정착
  - 콘텐츠 스레드
  - 콘텐츠 전이 규칙
  - 계측과 안정성 검증
- 채팅 서버 수직 흐름 정착
  - `ChattingServer`, `ChattingDummyClient`, `WinForms` 검증 경로 유지
- 로그인 플랫폼 정착
  - `LoginServer`, `MySQL`, `Redis`, `LoginAuth` 경계 유지
  - 중복 로그인 정책, ticket 수명, 운영 설정 정리
- 이후 `Gateway`, `WorldServer`, 인증 고도화 같은 역할 분리 검토

## 6. 현재 인증 아키텍처 요약
- `WinForms Client`
  - `LoginServer`에 HTTP로 로그인/회원가입 요청
- `LoginServer`
  - `MySQL`에서 계정 조회/생성
  - `Argon2id`로 비밀번호 해시 검증
  - `Redis`에 `chat:ticket:{ticket}`와 `chat:active-login:{userId}` 기록
- `ChattingServer`
  - `LoginAuthRq`에서 ticket를 consume
  - `loginVersion`으로 오래된 ticket를 거절
  - 같은 `userId`의 기존 세션을 찾아 강제 종료
- `Connector`
  - `ChattingServer`가 외부 저장소 세부 구현을 직접 들고 있지 않게 경계를 유지

자세한 내용은 [001_login-platform-overview.md](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/docs/architecture/004_login-platform/001_login-platform-overview.md)를 기준으로 본다.
