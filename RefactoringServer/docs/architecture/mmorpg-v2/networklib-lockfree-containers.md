# 락프리 컨테이너 1차 설계

## 1. 목적
- 새 `GameServer/NetworkLib`에서 사용할 공용 락프리 자료구조를 기존 포트폴리오 자산 기반으로 다시 정리한다.
- 컨테이너 이름, 네임스페이스, 헤더 경로를 새 코딩 컨벤션에 맞춘다.
- 첫 단계에서는 `queue`, `stack`, `memory pool`이 최소 컴파일과 기본 동작 검증을 통과하는 상태를 목표로 한다.

## 2. 이번 단계 범위
- `FLockFreeMemoryPool`
  - 태그드 포인터 기반 free-list로 정리
  - `Alloc`, `Free`, `GetCapacity`, `GetUseCount` 제공
- `FTlsMemoryPoolManager`
  - 기존 TLS 버킷 최적화 전체를 바로 이식하지 않고, 우선 `FLockFreeMemoryPool` 래퍼로 단순화
  - 이후 실제 contention 측정 뒤 TLS 버킷 최적화를 다시 올린다
- `FLockFreeStack`
  - 태그드 포인터 기반 `Push`, `Pop` 제공
- `FLockFreeQueue`
  - Michael-Scott queue 형태로 단일 dummy node를 사용하는 구조로 정리

## 3. 의도적인 보수화
- 기존 `CTLSSharedMemoryPool`의 버킷 공유 최적화는 이번 단계에서 보류한다.
- 이유는 새 솔루션 초기 상태에서 이름 정리와 컴파일 가능 상태를 먼저 확보하는 편이 위험이 낮기 때문이다.
- 즉, 이번 단계는 "기존 아이디어를 새 구조에 다시 앉히는 1차 정리"이며, 성능 최적화 최종본은 아니다.

## 4. 새 경로
- 루트: `GameServer/`
- 코어 라이브러리: `GameServer/NetworkLib/`
- 컨테이너 헤더:
  - `GameServer/NetworkLib/Include/NetworkLib/Containers/LockFreeCommon.h`
  - `GameServer/NetworkLib/Include/NetworkLib/Containers/FLockFreeStack.h`
  - `GameServer/NetworkLib/Include/NetworkLib/Containers/FLockFreeQueue.h`
- 메모리 헤더:
  - `GameServer/NetworkLib/Include/NetworkLib/Memory/FLockFreeMemoryPool.h`
  - `GameServer/NetworkLib/Include/NetworkLib/Memory/FTlsMemoryPool.h`

## 5. 다음 단계
- `FTlsMemoryPoolManager`에 실제 TLS 버킷 캐시를 다시 도입할지 결정
- `FLockFreeQueue` 멀티 프로듀서/멀티 컨슈머 반복 테스트 추가
- `NetworkLib` 송신 큐 후보로 바로 사용할 수 있도록 고정 길이 패킷 노드 타입 정의
