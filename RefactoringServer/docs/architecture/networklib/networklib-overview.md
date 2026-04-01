# NetworkLib Overview

## 1. 역할
- `NetworkLib`는 게임 로직을 직접 담는 라이브러리가 아니다.
- 목표는 `비동기 네트워크 처리`, `세션 수명주기`, `송수신 경계`, `락프리 자료구조 기반 유틸리티`를 제공하는 것이다.
- 상위 서버는 이 코어 위에 `IApplicationHandler`를 구현해서 붙는다.

## 2. 현재 공개 경계
- [`IServer.h`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Include\NetworkLib\IServer.h)
  - 서버 시작, 종료, 송신 인터페이스
- [`IApplicationHandler.h`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Include\NetworkLib\IApplicationHandler.h)
  - 서버 이벤트 콜백 인터페이스
- [`ServerFactory.h`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Include\NetworkLib\ServerFactory.h)
  - 백엔드 종류별 구현체 생성 경계
- [`BackendTypes.h`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Include\NetworkLib\BackendTypes.h)
  - 서버 설정과 백엔드 종류 정의

## 3. 현재 구현체
- [`FIocpServer.cpp`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Source\FIocpServer.cpp)
  - 실제 동작하는 첫 번째 백엔드
- [`FStubServer.cpp`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Source\FStubServer.cpp)
  - 미구현 백엔드 자리와 팩토리 검증용 스텁

## 4. 현재 설계 판단
- `NetworkLib`는 지금 `최소 서버 코어` 단계다.
- 패킷 프레이밍, 송신 큐, back-pressure, graceful shutdown, RIO/asio 구현은 아직 남아 있다.
- 따라서 현재 문서는 완성형 아키텍처가 아니라 `지금 코드가 어디까지 고정됐는지`를 설명하는 기준 문서로 본다.

## 5. 다음 확장 포인트
- 세션 핸들 모델 강화
- 송신 큐 도입
- 패킷 프레이밍 계층 추가
- 백엔드별 공통 테스트 시나리오 정의
