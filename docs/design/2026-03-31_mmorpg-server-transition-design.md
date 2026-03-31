# 설계 노트

## 1. 배경
- 현재 솔루션은 `Portfolio.sln` 기준으로 `NetworkLib`, `MonitoringClientLib`, `EchoServer` 세 프로젝트로 구성되어 있다.
- `EchoServer`는 `NetworkLib`와 `MonitoringClientLib`에 의존하는 샘플 서버 성격이 강하며, MMORPG 서버 구조로 확장하기에는 도메인 모델이 부족하다.
- 반면 `NetworkLib`에는 IOCP 기반 `AcceptEx`/`WSARecv`/`WSASend`, 세션 ID 관리, 콘텐츠 프레임 실행 구조가 이미 구현되어 있어 전부 폐기하기보다 재사용 가능성을 먼저 검토할 가치가 있다.
- 사용자 의도는 기존 포트폴리오용 Echo 서버를 정리하고, MMORPG 서버 개발을 위한 방향으로 저장소를 재편하는 것이다.

## 2. 목표
- `NetworkLib`와 `includes` 중 재사용 가능한 기반 기술을 남긴다.
- `EchoServer` 중심 구조를 MMORPG 서버용 실행 프로젝트 구조로 전환한다.
- 이후 리팩터링이 "기반 라이브러리 정리"와 "게임 서버 도메인 추가"로 분리되도록 경계를 명확히 한다.

## 3. 비목표
- 이번 문서 단계에서 전투, 이동, 인벤토리, AI 같은 MMORPG 게임플레이 시스템까지 확정하지 않는다.
- 현재 관찰만으로 `NetworkLib` 전체를 안전하다고 단정하지 않는다.
- 외부 DB, 로그인 서버, 월드 분리 구조는 아직 결정하지 않는다.

## 4. 현행 구조
- 현재 흐름:
  `EchoServer/Main.cpp`에서 초기화 후 `MonitoringClientLib` 연결을 시작하고, `CEchoServer`를 생성해 `NetworkLib::Core::Net::Server::CNetServer::Start()`를 호출한다.
- 관련 모듈:
  `NetworkLib/CNetServer.*`, `NetworkLib/CBaseContents.*`, `NetworkLib/CContentsThread.*`, `NetworkLib/ContentsFrameTask.*`, `includes/MHLib/*`
- 병목/제약:
  `NetworkLib`는 전역 객체(`g_NetServer`, `g_Logger`)와 강한 결합을 갖고 있다.
  세션 수명주기와 콘텐츠 이동이 `Interlocked`와 전역 세션 배열 접근에 의존한다.
  `SystemTask.cpp`에는 F1 종료, F2 프로파일 저장 같은 포트폴리오/로컬 실행 편의 로직이 포함되어 있어 실제 서버 런타임 정책과 분리할 필요가 있다.
  `MonitoringClientLib`는 유용할 수 있으나 MMORPG 서버 최소 코어를 세우는 첫 단계의 필수 요소인지는 재평가가 필요하다.

## 5. 제안 설계
### 5-1. 개요
- 저장소를 "기반 네트워크 계층"과 "MMORPG 서버 도메인 계층"으로 나눈다.
- 1차 목표는 Echo 전용 코드를 걷어내고, 월드 서버 골격을 올릴 수 있는 실행 프로젝트를 새로 세우는 것이다.
- `NetworkLib`는 당장 폐기하지 않고, 수명주기/의존성/전역 상태를 줄이는 방향으로 점진 리팩터링한다.

### 5-2. 데이터 흐름
1. 클라이언트 접속은 `CNetServer`가 수락하고 세션 ID를 발급한다.
2. 접속 세션은 월드/로비 등 상위 콘텐츠 객체로 이동한다.
3. 콘텐츠 객체는 패킷을 도메인 명령으로 해석하고 게임 상태를 갱신한다.
4. 응답 패킷 생성은 네트워크 전송 계층으로 다시 위임한다.

### 5-3. 주요 타입 / 인터페이스
- 유지 후보:
  `CNetServer`, `CNetSession`, `CSerializableBuffer`, `CRingBuffer`, `CLFQueue`, `CLFStack`, 메모리 풀 계열
- 정리 후보:
  `CEchoServer`, `CAuthContents`, `CEchoContents`, Echo 전용 프로토콜/설정/모니터
- 신규 후보:
  `WorldServer`, `LobbyContents`, `WorldContents`, `PlayerSessionContext`, `PacketDispatcher`

### 5-4. 스레드 / 락 / 생명주기 고려사항
- `CNetSession::m_iIOCountAndRelease` 기반 해제는 이미 구현되어 있으므로, 1차 전환에서는 이 계약을 깨지 않는 범위에서 작업한다.
- `CBaseContents`는 세션 배열과 전역 서버에 직접 접근하므로 테스트와 모듈성 측면에서 결합도가 높다.
- MMORPG 구조로 가려면 "세션의 네트워크 생명주기"와 "플레이어 도메인 객체 생명주기"를 분리하는 작업이 필요하다.
- 콘텐츠 이동 시 `void* objectPtr`를 사용하는 현재 구조는 타입 안정성이 낮으므로 추후 명시적 컨텍스트 타입으로 교체를 검토한다.

### 5-5. 실패 처리
- 네트워크 오류와 프로토콜 검증 실패는 지금처럼 세션 종료가 기본 정책이 될 수 있다.
- 다만 MMORPG 서버에서는 종료 사유 로깅, 비정상 패킷 카운팅, 인증 이전/이후 정책 분리가 필요하다.
- 현재는 `OnError()` 활용도가 낮아 보여, 전환 과정에서 오류 보고 경로를 재설계해야 한다.

## 6. 대안 비교
### 대안 A
- 내용:
  `NetworkLib`를 유지하고 Echo 전용 프로젝트만 MMORPG용으로 교체한다.
- 장점:
  이미 있는 IOCP/세션/버퍼/콘텐츠 루프를 활용할 수 있다.
  포트폴리오에서 "직접 구현한 서버 기반 기술"을 살리기 좋다.
- 단점:
  기존 결합도와 수명주기 복잡도를 함께 안고 간다.

### 대안 B
- 내용:
  `EchoServer`뿐 아니라 `NetworkLib`도 상당 부분 폐기하고 MMORPG 서버를 처음부터 다시 구성한다.
- 장점:
  구조를 더 현대적으로 정리하기 쉽다.
  전역 상태와 샘플 코드 흔적을 빠르게 제거할 수 있다.
- 단점:
  기존 자산의 강점이 사라지고, 작업량과 리스크가 크게 증가한다.
  현재 수집된 근거만으로는 전면 폐기 판단이 이르다.

### 선택 이유
- 현재는 대안 A를 기본 전략으로 잡는다.
- 이유는 `NetworkLib`에 이미 서버 코어로서 의미 있는 구현이 존재하고, 사용자도 `NetworkLib`와 `includes` 재사용 가능성을 언급했기 때문이다.
- 단, 이 선택은 "전부 유지"가 아니라 "근거 기반 선별 유지"를 의미한다.

## 7. 검증 계획
- 빌드 검증:
  `Portfolio.sln`의 x64 Debug 기준으로 현재 빌드 가능 여부를 먼저 확인한다.
- 기능 검증:
  접속, 패킷 송수신, 세션 종료, 콘텐츠 이동의 최소 시나리오를 확인한다.
- 부하 / 성능 검증:
  전환 초기에는 생략 가능하지만, 이후 더미 클라이언트 기반 접속/송수신 테스트가 필요하다.
- 로그 확인 포인트:
  Accept/Recv/Send 오류 로그, 세션 카운트 변화, 콘텐츠 FPS/지연 프레임

## 8. 리스크
- 기술 리스크:
  세션 해제 타이밍과 콘텐츠 처리 간 경합 조건이 숨어 있을 수 있다.
- 운영 리스크:
  키보드 입력 기반 종료 같은 로컬 제어 방식은 실제 서버 운영 모델과 맞지 않는다.
- 추후 가능성:
  로그인/로비/월드 분리, DB 연동, 패킷 디스패처, ECS 또는 영역 기반 월드 관리로 확장 가능하다.

## 9. 결론
### 확정된 사실
- 현재 솔루션은 `EchoServer`가 실행 프로젝트이고 `NetworkLib`가 핵심 네트워크 라이브러리 역할을 한다.
- `NetworkLib`에는 IOCP 기반 서버 루프와 콘텐츠 프레임 실행 구조가 존재한다.
- `EchoServer`는 MMORPG 도메인으로 확장하기보다 샘플/실험 코드 성격이 강하다.

### 가설 또는 불확실성
- `NetworkLib`의 동시성 안정성이 실제 MMORPG 서버 기준으로 충분한지는 아직 확인되지 않았다.
- `MonitoringClientLib`를 얼마나 유지할지는 현재 정보만으로 확정하기 어렵다.
- `includes/MHLib`의 일부 유틸리티가 현재도 적극적으로 유지 가능한 품질인지는 추가 검토가 필요하다.

### 후속 작업
- `NetworkLib` 핵심 클래스의 수명주기와 동시성 위험 지점을 코드 리뷰 문서로 정리한다.
- `EchoServer`를 대체할 MMORPG 서버 실행 프로젝트 구조를 설계한다.
- 이후 실제 코드 변경은 "Echo 제거 또는 격리"부터 시작한다.
