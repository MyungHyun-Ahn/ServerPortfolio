# Plan Status Board

## 1. Status Legend
- `Completed`
  - First implementation and baseline verification are done.
- `In Progress`
  - Implementation or verification is still ongoing.
- `Needs Follow-up`
  - Direction is agreed, but the work is intentionally deferred.

## 2. Work Streams
| ID | Area | Status | Notes |
|---|---|---|---|
| `001` | Foundation | In Progress | `Diagnostics` RTT metrics, `Ids` allocator, and YAML `Config` + `ConfigGenerator` are in place. Remaining work is boundary cleanup between `Logging / Diagnostics / Config` and reserve-bit follow-up policy. |
| `002` | Legacy MhLib Review | Completed | Legacy structure comparison and reference notes are organized. |
| `003` | NetworkLib Crypto / Packet Header | Completed | Cipher, framing, and content header baseline are organized. |
| `004` | NetworkLib Session | Completed | Session lifecycle and ownership model are organized. |
| `005` | NetworkLib Packet View | Completed | `string_view`, `bytes_view`, and borrowed-view guard work is done. |
| `006` | Packet Schema Tooling | Completed | `PacketGenerator` and generated packet/handler/router flow are in place. |
| `007` | NetworkLib Performance | In Progress | `IOCP + RIO` dual-backend split, pure `RIO` baseline, `Rio Direct / Rio OwnerThread / Iocp` comparisons, `SO_SNDBUF` comparisons, `SendPacket` path rewrite, and `IOCP AcceptEx` migration are done. Current baseline on the same machine favors `Rio Direct`, but results should still be treated as relative rankings because server and client were co-located during the tests. |
| `008` | ContentsRuntime | In Progress | Lock-free inbox validation, lobby/room multi-instance flow, send lost-wakeup fix, `contentInstanceId` allocator, and the first `content worker pool` conversion are done. The old `content instance = dedicated thread` model is gone, and `ContentsWorkerThreadCount` now controls the worker pool size. Remaining work is placement policy refinement and future multi-content expansion. |
| `009` | WorldServer | Needs Follow-up | Cell-based world simulation and task-graph execution are separated into a future track. Direction is `ContentsRuntime = executor`, `WorldContent = task graph owner`, but priority is intentionally very low for now. |

## 3. Current Priorities
1. `008_contents-runtime`
   - Stabilize the new content worker pool structure.
   - Refine placement policy after the `instance-thread` split.
   - Re-run network benchmarks on top of the new runtime baseline.
2. `007_networklib-performance`
   - Continue `RIO` follow-up optimization.
   - Add broadcast fan-out and further send-copy reduction.
   - Compare `IOCP AcceptEx` performance and revisit socket-reuse only if needed.
3. `001_foundation`
   - Clean up `Logging / Diagnostics / Config` boundaries.
   - Finalize the follow-up policy for `contentInstanceId` reserve bits.
4. Low-priority expansion tracks
   - Resume multi-content runtime expansion when new content types are actually added.
   - Start `WorldServer` task-graph work only when real world-content requirements appear.

## 4. Follow-up Backlog
- `007_networklib-performance`
  - Page-pool remeasurement.
  - Large-payload copy-reduction validation.
  - `RIO` buffer registration/release cost tuning.
  - Broadcast packet fan-out verification.
  - `IOCP` send-path copy reduction vs `SO_SNDBUF=0` trade-off review.
- `008_contents-runtime`
  - Worker-pool placement tuning.
  - Additional content types and multi-content expansion.
  - Normal-failure vs abnormal-failure logging policy cleanup.
- `009_worldserver`
  - Cell task graph model.
  - One-shot task, phase, and barrier execution model.
  - World-content internal scheduler design.
- `001_foundation`
  - Server-side RTT/diagnostics reuse boundary cleanup.
  - Future reserve-bit transition policy such as `serverId` if distributed servers become real scope.
