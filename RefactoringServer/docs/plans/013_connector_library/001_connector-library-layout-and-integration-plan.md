# Connector 라이브러리 구성 및 연동 계획

상태: 활성  
정본: 예  
최종 갱신: 2026-04-09  
범위: `Libraries/Connector` 구성과 `Redis/MySQL` 연동 경계

## 1. 목표
- `Redis`, `MySQL` 같은 외부 저장소 의존성을 `NetworkLib`, `ContentsRuntime` 밖으로 분리한다.
- `ChattingServer`가 이후 `ticket login`을 붙일 때, 외부 인증 저장소 접근을 별도 라이브러리 경계로 주입할 수 있게 만든다.
- 현재 [`Libraries/includes/connector`](D:\Project\ServerPortfolio\RefactoringServer\Libraries\includes\connector)의 예전 `MySQL`, `Redis` 커넥터 코드를 참고하되, 현재 코드베이스 스타일에 맞는 새 래퍼 구조로 재정리한다.

## 2. 현재 판단
- `NetworkLib`는 transport, packet, session, backend 책임에 집중해야 한다.
- `ContentsRuntime`는 mailbox, worker executor, routing 책임에 집중해야 한다.
- `Redis`, `MySQL` 연결 코드는 위 두 계층보다 상위의 application/service 관심사다.
- 따라서 새 외부 저장소 계층은 `Libraries/Connector`로 분리하는 것이 가장 자연스럽다.

## 3. 왜 `NetworkLib`에 넣지 않는가
- `NetworkLib`는 패킷 framing, checksum, cipher, send/recv path 같은 핵심 네트워크 경로를 담당한다.
- 여기에 `Redis`, `MySQL`, 계정 인증, ticket 조회 같은 외부 I/O를 섞으면 책임이 흐려진다.
- 장기적으로 `EchoServer`, `ChattingServer`, 이후 다른 서버 샘플이 모두 같은 `NetworkLib`를 써야 하므로, 특정 인증/계정 저장소 의존은 코어 계층에 들어가면 안 된다.

## 4. 왜 `ContentsRuntime`에 넣지 않는가
- `ContentsRuntime`는 content execution model과 mailbox owner transfer 규칙을 제공하는 실행기다.
- 외부 저장소 client 구현을 `ContentsRuntime`에 넣기 시작하면, 실행기 계층이 인프라 어댑터까지 알게 된다.
- 올바른 방향은 `Contents` 또는 application 조립 지점이 `Connector` 인터페이스를 주입받아 사용하는 것이다.

## 5. 예전 커넥터 코드에서 참고할 점과 그대로 쓰기 어려운 점
참고 소스:
- [`CDBConnector.h`](D:\Project\ServerPortfolio\RefactoringServer\Libraries\includes\connector\CDBConnector.h)
- [`CRedisConnector.h`](D:\Project\ServerPortfolio\RefactoringServer\Libraries\includes\connector\CRedisConnector.h)

참고할 수 있는 점:
- `thread_local` 연결 재사용 아이디어
- `Redis set/get/del`, `MySQL query/result`의 최소 기능 범위
- 이후 로그인 서버 연동 시 필요한 저장소 종류를 빠르게 파악할 수 있다.

그대로 재사용하기 어려운 이유:
- `g_Logger`, `MYSQL_SETTING`, `REDIS_SETTING`, `CEncodingConvertor` 같은 전역 의존이 강하다.
- 헤더 단일체 구조라 빌드 경계와 의존 경계가 불분명하다.
- `WCHAR` 포맷 query, 고정 크기 버퍼, 예외/로그 혼합 방식이 현재 공용 라이브러리 스타일과 맞지 않는다.
- `ChattingServer`가 원하는 것은 계정 DB 일반 기능 전체가 아니라, 1차적으로 `ticket consume` 같은 좁은 capability다.

## 6. 결정
- 새 정적 라이브러리 프로젝트 `Libraries/Connector`를 만든다.
- 의존 방향은 아래로 고정한다.

```text
Foundation
  ^
  |
Connector
  ^
  |
ChattingServer / future servers / tools
```

- `Connector`는 `Foundation`에는 의존할 수 있다.
- `Connector`는 `NetworkLib`, `ContentsRuntime`, packet generated code에는 의존하지 않는다.
- `NetworkLib`, `ContentsRuntime`는 `Connector`를 참조하지 않는다.

## 7. 1차 범위
### 7.1 바로 필요한 것
- `Redis` 기반 chat ticket 조회/소비용 인터페이스
- `ChattingServer`가 주입 가능한 `Null` 또는 `InMemory` 구현
- `cpp_redis` 기반 실제 adapter

### 7.2 아직 급하지 않은 것
- `MySQL` 계정 repository 구현
- 공통 connector executor 또는 비동기 DB runtime
- 전체 vendor 디렉터리 재배치

### 7.3 1차 원칙
- `ChattingServer`는 우선 `Redis ticket consume`만 사용한다.
- `MySQL AccountDB`는 계획대로 `Node.js LoginServer`에서 먼저 사용한다.
- 즉 C++ 서버 1차 목적은 `외부 인증 검증 전체`가 아니라 `ticket 소비`다.

## 8. 추천 디렉터리 구조
```text
Libraries/
  Connector/
    Connector.vcxproj
    Pch.h
    Pch.cpp
    Config/
      ConnectorTypes.h
      RedisConnectorConfig.h
      MySqlConnectorConfig.h
    Interfaces/
      IChatTicketStore.h
      IAccountRepository.h
    Redis/
      FCppRedisChatTicketStore.h
      FCppRedisChatTicketStore.cpp
      FNullChatTicketStore.h
      FNullChatTicketStore.cpp
      FInMemoryChatTicketStore.h
      FInMemoryChatTicketStore.cpp
    MySql/
      FMySqlAccountRepository.h
      FMySqlAccountRepository.cpp
    Testing/
      ConnectorTestTypes.h
```

1차에서는 `IChatTicketStore`, `FNullChatTicketStore`, `FInMemoryChatTicketStore`, `FCppRedisChatTicketStore`까지만 먼저 구현한다.

## 9. 인터페이스 설계 기준
- 전역 singleton 대신 명시적 객체 소유권을 사용한다.
- logger는 `Foundation::ILogger`를 주입받는다.
- 설정값은 전역 상수 대신 struct config로 전달한다.
- public API는 `ticket issue/consume` 같은 사용 목적 중심 capability를 노출한다.
- `Redis client`나 `MYSQL*` 같은 vendor type은 public header로 최대한 새지 않게 한다.
- 에러 전달은 현재 코드베이스 스타일에 맞춰 `bool + outError` 또는 명확한 반환 구조체를 사용한다.

예시:
```cpp
namespace Connector
{
    struct SConsumedChatTicket
    {
        std::uint32_t userId = 0;
        std::string loginId;
        bool valid = false;
    };

    class IChatTicketStore
    {
    public:
        virtual ~IChatTicketStore() = default;

        virtual bool TryConsumeChatTicket(
            std::string_view ticket,
            SConsumedChatTicket& outTicket,
            std::string& outError) = 0;
    };
}
```

## 10. 조립 위치
- 실제 connector 객체 생성은 [`ChattingServer/Main.cpp`](D:\Project\ServerPortfolio\RefactoringServer\Chatting\ChattingServer\Main.cpp) 같은 application 조립 지점에서 한다.
- `FAuthContent` 또는 별도 `AuthService`는 `IChatTicketStore` 인터페이스만 의존한다.
- `Main.cpp`가 config를 읽고 `Null`, `InMemory`, `Redis` 구현 중 하나를 선택해 주입하는 방식이 적합하다.

## 11. `FAuthContent`와의 연결 방향
1. 기존 `legacy insecure login` 경로는 그대로 유지한다.
2. 이후 `ticket login packet`을 추가한다.
3. `FAuthContent`는 packet 종류에 따라
   - 기존 `userId != 0` 로그인
   - `IChatTicketStore` 기반 ticket 소비 로그인
   두 경로를 병행 처리한다.
4. ticket 검증 성공 시 기존과 동일하게 session을 `Lobby`로 이동시킨다.

## 12. 동기/비동기 정책
- 1차 프로토타입에서는 `AuthContent`에서 짧은 `Redis` consume를 동기 호출하는 것을 허용한다.
- 단, 이 호출은 `room chat hot path`가 아니라 `login auth path`에만 둔다.
- 장기적으로 로그인 트래픽이 커지면 아래 중 하나로 확장한다.
  - dedicated connector worker
  - async callback 기반 bridge
  - auth request queue + completion event

즉 1차는 단순성과 빠른 통합을 우선하고, blocking I/O 범위를 `auth` 구간으로 제한한다.

## 13. third-party 정리 방침
### 13.1 1차
- 현재 `Libraries/includes/cpp_redis`, `Libraries/includes/tacopie`, `Libraries/includes/MySQL`은 그대로 참고/재사용 가능하게 둔다.
- 단, 새 production code는 `Libraries/includes/connector`의 예전 wrapper를 직접 include하지 않는다.
- 새 `Connector` 프로젝트가 vendor header를 직접 감싸는 형태로 간다.

### 13.2 이후 후속 작업
- vendor 의존이 안정화되면 `Libraries/includes`를 `ThirdParty`류 디렉터리로 재배치하는 별도 정리 작업을 고려한다.
- 이 작업은 `Connector` 1차 스캐폴드 이후 별도 과제로 분리한다.

## 14. 단계별 구현 계획
1. `Libraries/Connector` 프로젝트 생성
2. `Foundation` 의존과 include path만 연결한 빈 static library 빌드 확인
3. `IChatTicketStore` 인터페이스 추가
4. `Null`, `InMemory` 구현 추가
5. `cpp_redis` 기반 `FCppRedisChatTicketStore` 추가
6. `ChattingServer` config에 connector 관련 설정 추가
7. `Login packet`에 ticket login 메시지 추가
8. `FAuthContent` 또는 별도 auth service에 ticket 검증 경로 주입
9. `legacy insecure login`과 `ticket login` 병행 smoke test

## 15. 1차 완료 기준
- `Connector.vcxproj`가 독립 빌드된다.
- `ChattingServer`는 connector 미사용 모드에서도 기존처럼 동작한다.
- `Null` 또는 `InMemory` 구현으로 local smoke test가 가능하다.
- `Redis` 기반 ticket consume 성공/실패가 로그인 결과에 반영된다.
- `NetworkLib`, `ContentsRuntime`에는 connector 의존이 추가되지 않는다.

## 16. 하지 않을 것
- `NetworkLib` 내부에 `Redis/MySQL` 코드 삽입
- `ContentsRuntime` 내부에 connector client 내장
- 1차에서 `MySQL` 계정 저장/조회까지 `ChattingServer`가 직접 담당
- 1차에서 전체 connector를 범용 async job system으로 과설계

## 17. 정리
- 현재 구조에서 `Connector`는 `Libraries` 아래 새 프로젝트로 분리하는 것이 가장 안전하다.
- `ChattingServer`는 우선 `Redis ticket consume`만 붙이고, `MySQL`은 `Node.js LoginServer`가 먼저 담당한다.
- 예전 커넥터 코드는 reference로만 보고, 새 `Connector` 라이브러리에서 현재 코드베이스 스타일에 맞는 얇은 adapter로 다시 감싸는 방향으로 간다.
