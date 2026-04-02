# C++ Header / PCH Convention

## 1. 목적
- 헤더 의존성을 줄여 빌드 영향 범위를 작게 유지한다.
- 자주 바뀌지 않는 STL/Windows 의존성과 `NetworkLib` 공용 타입 묶음은 PCH로 올린다.
- 공개 인터페이스는 forward declaration 우선, 구현 상세는 PCH 우선으로 정리한다.

## 2. 기본 원칙
- `.cpp` 파일은 자기 모듈의 `Pch.h`를 가장 먼저 include한다.
- 헤더 파일에서는 `#include`를 기본값으로 두지 않는다.
- 먼저 forward declaration 또는 PCH로 해결 가능한지 확인한다.
- 아래 경우에만 헤더 include를 예외적으로 허용한다.
  - 기반 클래스 정의가 직접 필요한 경우
  - 값 멤버로 완전형이 필요한 경우
  - 템플릿/inline 구현 때문에 완전형이 필요한 경우
  - 공개 API 의미상 실제 타입 노출이 꼭 필요한 경우

## 3. NetworkLib 공용 PCH
- `NetworkLib` 공용 묶음은 [`NetLibPch.h`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\NetLibPch.h)에서 관리한다.
- 소비 프로젝트는 자기 `Pch.h`에서 `#include "NetLibPch.h"` 한 줄로 공용 `NetworkLib` PCH를 가져온다.
- `Packet` 하위 헤더는 현재 규칙상 project include를 두지 않고 `NetLibPch.h` 기반으로 동작한다.

## 4. NetworkLib 헤더 규칙
- `NetworkLib/Packet/*`
- `NetworkLib/Memory/*`
- `NetworkLib/Crypto/*`
- `NetworkLib/Containers/*`

위 저수준 계층 헤더는 현재 규칙상 project include를 두지 않는다.

## 5. Packet 헤더 규칙
- `Packet/Buffer/*`
- `Packet/Framing/*`
- `Packet/Serialization/*`
- `Packet/View/*`

위 헤더들은 project include를 두지 않는다.

- 필요한 타입은 `NetLibPch.h`에서 선행 include한다.
- PCH를 쓰는 일반 프로젝트는 추가 include 없이 packet 헤더를 사용할 수 있다.
- PCH를 쓰지 않는 예외 프로젝트는 `Main.cpp` 같은 진입 파일에서 `NetLibPch.h`를 먼저 include해야 한다.

## 6. 공개 인터페이스 규칙
- `Servers/IServer.h`, `Servers/IApplicationHandler.h` 같은 공개 인터페이스는 forward declaration 우선이다.
- 구현 디렉터리인 `Servers/Core/*`, `Servers/Session/*`, `Packet/*`은 PCH 기반으로 최대한 가볍게 유지한다.

## 7. 금지 사항
- 같은 STL/Windows include를 헤더마다 반복해서 넣지 않는다.
- forward declaration으로 충분한 대상을 습관적으로 include하지 않는다.
- 이유 없이 헤더 include를 늘리지 않는다.

## 8. 리뷰 체크리스트
- 이 include가 정말 헤더에서 필요한가
- forward declaration으로 대체 가능한가
- `NetLibPch.h`로 올릴 수 있는 안정적인 의존성인가
- PCH 없는 예외 프로젝트가 있다면 `NetLibPch.h` 선행 include가 보장되는가
