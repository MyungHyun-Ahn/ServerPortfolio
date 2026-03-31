# 기획 노트

## 1. 배경
- 현재 실행 프로젝트인 `EchoServer`는 `NetworkLib` 기반 서버 실행 흐름을 보여주지만, 모니터링 클라이언트 초기화와 Echo 전용 콘텐츠 구성이 함께 들어 있어 MMORPG 서버 시작점으로 쓰기에는 책임이 섞여 있다.
- `Main.cpp` 기준으로 보면 실행 순서는 다음과 같다.
  - 설정 초기화
  - LAN/모니터링 클라이언트 초기화
  - `g_NetServer = new CEchoServer`
  - `Start(openIP, openPort)`
- `CEchoServer`는 `OnAccept()`에서 인증 콘텐츠로 세션을 이동시키고, `RegisterContentTimerEvent()`에서 인증/에코 콘텐츠를 각각 생성해 프레임 태스크에 등록한다.
- 이 구조는 `WorldServer` 골격으로 바꾸기 위한 출발점으로는 충분하지만, 모니터링과 Echo 콘텐츠를 걷어내고 "빈 월드 서버" 형태로 다시 정리할 필요가 있다.

## 2. 목적
- `EchoServer`를 직접 확장하지 않고, MMORPG 전환용 새 실행 프로젝트 `WorldServer`의 최소 골격을 설계한다.
- 첫 버전의 `WorldServer`는 네트워크 수락, 초기 콘텐츠 진입, 빈 프레임 루프까지 제공하는 것을 목표로 한다.
- 실제 게임 로직 추가 전, 빌드와 접속 검증이 가능한 최소 서버 시작점을 확보한다.

## 3. 목표 범위
### 3-1. 포함
- 새 콘솔 실행 프로젝트 `WorldServer`
- `CNetServer` 상속 서버 클래스
- 최소 1개 이상의 콘텐츠 클래스
- 설정 파일과 초기화 코드
- `TestClient`로 접속/로그인 수준의 최소 검증 가능 상태

### 3-2. 제외
- 실제 월드 상태 관리
- 플레이어 이동, 전투, NPC, 인벤토리
- DB 연동
- 모니터링 클라이언트 연결
- Echo 전용 패킷 처리 유지

## 4. 제안 구조
### 4-1. 프로젝트 구성
- `WorldServer/`
  - `Main.cpp`
  - `CWorldServer.h`
  - `CWorldServer.cpp`
  - `CEntranceContents.h`
  - `CEntranceContents.cpp`
  - `WorldServerSetting.h`
  - `WorldServerSetting.cpp`
  - `ServerConfig.conf`
  - `pch.h`
  - `pch.cpp`

### 4-2. 초기 실행 흐름
1. `WorldServer` 초기화 함수 호출
2. `ServerConfig.conf` 로드
3. `g_NetServer = new CWorldServer`
4. `Start(openIP, openPort)` 호출
5. `RegisterContentTimerEvent()`에서 `CEntranceContents` 등록
6. `OnAccept()`에서 신규 세션을 `CEntranceContents`로 이동

## 5. 클래스 역할 초안
### 5-1. `CWorldServer`
- 역할:
  - `CNetServer` 상속 구현
  - 접속 허용 여부 결정
  - accept 후 초기 콘텐츠 진입
  - leave와 error 처리의 기본 정책 유지
  - 콘텐츠 프레임 태스크 등록
- 첫 버전 구현 원칙:
  - `EchoServer`보다 단순하게 시작
  - 모니터링 초기화 없음
  - Echo 콘텐츠 생성 없음

### 5-2. `CEntranceContents`
- 역할:
  - 신규 세션의 첫 진입 지점
  - 아직 월드 로직이 없어도 세션을 안전하게 보유
  - 이후 인증/로비/월드 콘텐츠 분리 전까지 임시 진입 콘텐츠 역할
- 첫 버전 구현 원칙:
  - `OnEnter()`에서 세션 컨텍스트를 최소 생성하거나 null 상태 허용
  - `OnRecv()`는 당장 복잡한 해석 대신 최소 패킷만 처리하거나, 테스트용 연결 유지 정책만 유지
  - `OnLoopEnd()`는 비워둬도 됨

### 5-3. `WorldServerSetting`
- 역할:
  - 포트, openIP, 콘텐츠 FPS 같은 실행 파라미터 보관
  - `EchoServerSetting` 의존 제거

## 6. 재사용 기준
### 6-1. 그대로 재사용할 것
- `NetworkLib` 전체 서버 시작 구조
- `ContentsFrameTask`
- `CContentsThread`
- `CSerializableBuffer`, `CRingBuffer`, `CLFQueue` 등 기반 자료구조
- `TestClient` 기반 접속 검증 루프

### 6-2. 복사 후 정리할 것
- `EchoServer` 프로젝트 파일 설정
- `pch` 구성
- 설정 파일 로딩 패턴

### 6-3. 가져오지 않을 것
- `MonitoringClientLib` 초기화
- `CEchoContents`, `CAuthContents`
- Echo 전용 패킷 코드와 응답 흐름
- F1/F2 중심 샘플 제어 흐름을 전제로 한 동작

## 7. 첫 구현 단계 제안
### 7-1. 1차 목표
- `WorldServer` 프로젝트 생성
- 빌드 성공
- `Start()`까지 실행 가능
- 신규 세션 accept 시 `CEntranceContents`로 이동

### 7-2. 2차 목표
- `TestClient`가 접속 후 비정상 종료 없이 유지
- 최소 패킷 1개 처리 또는 무시 정책 정리

### 7-3. 3차 목표
- `Entrance -> Lobby/World`로 갈 수 있는 확장 지점 문서화

## 8. 검증 계획
- 빌드:
  - `Portfolio.sln`에 `WorldServer` 추가 후 x64 Debug 빌드
- 런타임:
  - `WorldServer` 실행
  - `TestClient`로 최소 접속 확인
- 후속 검증:
  - 필요 시 `TestClient`를 `WorldServer`용 최소 패킷 규약에 맞게 조정

## 9. 리스크
- 현재 `TestClient`는 Echo 로그인/에코 규약을 사용하므로, `WorldServer` 초기 버전이 같은 패킷 규약을 제공하지 않으면 테스트 클라이언트도 함께 조정해야 한다.
- `CEntranceContents`를 너무 비워두면 접속 직후 들어오는 패킷의 처리 정책이 모호해질 수 있다.
- `EchoServer` 프로젝트 설정을 그대로 복사하면 샘플 코드 의존이 남을 수 있으므로, 포함 파일 목록을 명시적으로 줄여야 한다.

## 10. 결론
### 확정된 사실
- `WorldServer`는 `EchoServer`를 직접 확장하기보다 별도 실행 프로젝트로 만드는 편이 구조적으로 안전하다.
- 첫 버전은 "빈 월드 진입 서버" 정도의 골격만 있어도 다음 리팩터링의 기준점이 된다.

### 다음 확인 항목
- `WorldServer` 첫 버전에서 `TestClient` 호환 패킷을 임시 유지할지 결정
- `CEntranceContents`에서 세션 컨텍스트를 바로 둘지 결정
- 실제 프로젝트 생성 작업으로 바로 넘어갈지 결정
