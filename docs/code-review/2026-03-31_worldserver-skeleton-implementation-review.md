# 코드 리뷰 노트

## 1. 작업 개요
- 작업명: `WorldServer` 실행 골격 1차 구현
- 관련 문서:
  - `docs/design/2026-03-31_worldserver-minimum-interface-design.md`
  - `docs/design/2026-03-31_worldserver-skeleton-design.md`
  - `docs/design/2026-03-31_networklib-safe-refactoring-guideline.md`
- 대상 파일:
  - `WorldServer/WorldServer.vcxproj`
  - `WorldServer/Main.cpp`
  - `WorldServer/CWorldServer.h`
  - `WorldServer/CWorldServer.cpp`
  - `WorldServer/CEntranceContents.h`
  - `WorldServer/CEntranceContents.cpp`
  - `WorldServer/CGenPacket.h`
  - `WorldServer/CGenPacket.cpp`
  - `WorldServer/WorldProtocol.h`
  - `WorldServer/CPlayerSessionContext.h`
  - `WorldServer/WorldServerSetting.h`
  - `WorldServer/WorldServerSetting.cpp`
  - `WorldServer/LoadConfig.h`
  - `WorldServer/InitWorldServer.h`
  - `WorldServer/pch.h`
  - `WorldServer/pch.cpp`
  - `Portfolio.sln`
- 목적:
  - `EchoServer`와 별도의 MMORPG 전환용 실행 프로젝트 골격을 만들고,
  - 최소 접속/로그인/에코 검증이 가능한 시작점을 확보한다.

## 2. 변경 요약
- 새 콘솔 프로젝트 `WorldServer`를 솔루션에 추가했다.
- `CWorldServer`를 `CNetServer` 상속 클래스로 구현해 접속 허용, accept 후 초기 콘텐츠 진입, 콘텐츠 프레임 등록을 담당하게 했다.
- `CEntranceContents`를 추가해 신규 세션의 첫 진입 지점으로 사용했다.
- `TestClient` 런타임 검증을 위해 로그인/에코/heartbeat만 임시 호환 규약으로 처리했다.
- `WorldServer`는 모니터링 클라이언트 없이 `NetworkLib`만 링크하도록 분리했다.

## 3. 판단 근거
- `EchoServer/Main.cpp`는 모니터링 초기화와 Echo 콘텐츠 구성이 함께 들어 있어 MMORPG 서버 시작점으로 쓰기에는 책임이 섞여 있었다.
- `CEchoServer`의 구조를 보면 실행 프로젝트가 실제로 해야 할 최소 책임은 `CNetServer` 가상 함수 구현과 콘텐츠 등록이라는 점이 분명했다.
- 따라서 `WorldServer`는 처음부터 `EchoServer`를 수정하기보다 별도 프로젝트로 만드는 편이 구조적으로 안전했다.
- 다만 현재 검증 자산은 `TestClient`뿐이므로, 첫 버전은 최소 로그인/에코 호환을 임시로 유지하는 것이 재현 가능성과 검증 가능성 측면에서 유리했다.

## 4. 구현 포인트
- `Main.cpp`
  - `InitWorldServer()`와 `LoadConfig()`를 통해 최소 초기화만 수행한다.
  - `Start()` 실패 시 바로 종료해 바인딩 실패를 숨기지 않도록 했다.
- `LoadConfig.h`
  - skeleton 단계에서는 인코딩/파서 리스크를 줄이기 위해 기존에 검증된 `..\EchoServer\ServerConfig.conf`를 재사용한다.
- `CWorldServer`
  - `OnAccept()`에서 세션을 `CEntranceContents`로 이동시킨다.
  - `RegisterContentTimerEvent()`에서 `CEntranceContents` 1개만 등록한다.
- `CEntranceContents`
  - 세션 컨텍스트를 생성/해제한다.
  - 로그인 요청에 대해 성공 응답을 보낸다.
  - 에코 요청은 그대로 되돌려준다.
  - heartbeat는 수신 시각만 갱신한다.

## 5. 검증 결과
- 빌드 검증:
  - `Portfolio.sln` x64 Debug 빌드 성공
  - 산출물 `Out/WorldServer.exe` 생성 확인
- 런타임 검증:
  - `WorldServer.exe` 실행
  - `TestClient --clients 1 --repeat 1 --hold-seconds 1 --heartbeat-ms 1000 --no-wait`
  - 결과:
    - 서버 접속 성공
    - 로그인 성공
    - 에코 1회 성공
    - heartbeat 성공
    - 최종 출력 `모든 테스트가 성공했습니다.`

## 6. 남은 리스크
- `WorldServer`는 아직 `EchoServer` 설정 파일을 재사용한다.
- `CEntranceContents`의 패킷 규약은 임시 검증용이므로, 추후 `WorldServer` 고유 프로토콜 또는 로비/월드 구조로 정리해야 한다.
- `Main.cpp`는 현재 `Sleep(INFINITE)`로 프로세스를 유지하므로, 정상 종료 경로는 아직 skeleton 수준이다.
- `LoadConfig.h`에는 인코딩 경고(C4819)가 남아 있어 추후 파일 인코딩 정리가 필요하다.

## 7. 결론
### 확정된 사실
- `WorldServer`는 별도 실행 프로젝트로 실제 빌드 및 실행 가능한 상태까지 도달했다.
- 현재 `NetworkLib` 기반 골격만으로도 `TestClient` 최소 검증 루프를 통과한다.

### 다음 확인 항목
- `WorldServer` 전용 설정 파일을 인코딩 포함해 독립시키기
- `CEntranceContents`를 로비/월드 진입 구조로 분리할지 결정
- 임시 로그인/에코 호환 규약을 언제 제거할지 결정
