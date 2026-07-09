# niniBUS Design Decisions

This document records the important design decisions behind `niniBUS`.

The goal is not to prove every decision is perfect. The goal is to explain why
the current design exists, what tradeoffs it accepts, and when a decision should
be reconsidered.

## Decision Format

Each decision uses:

- **Status**: accepted, current, deferred, or open.
- **Decision**: what the project is choosing.
- **Rationale**: why this choice fits the current milestone.
- **Consequences**: what this makes easier or harder.
- **Revisit when**: conditions that should reopen the decision.

## DD-001 - Embedded-Friendly Core

**Status**: accepted

**Decision**: `niniBUS` should keep an embedded-friendly core: small API,
predictable behavior, simple data structures, and minimal runtime assumptions.

**Rationale**:

- The bus should be easy to integrate into small C++ projects.
- The core should remain understandable before adding optimization or transport
  layers.
- Embedded-oriented constraints encourage clear ownership and bounded feature
  scope.

**Consequences**:

- V0 avoids IPC, dynamic plugins, persistence, and background worker threads.
- Optimizations are deferred until there are measurements.
- The public API should stay small.

**Revisit when**:

- The project formally targets desktop/server use first.
- The API needs features that conflict with embedded constraints.

## DD-002 - V0 Is In-Process Only

**Status**: accepted

**Decision**: V0 is an in-process message bus.

**Rationale**:

The first milestone should validate the core lane/message abstraction without
mixing in unrelated IPC concerns such as serialization, sockets, shared memory,
process lifetime, and connection management.

**Consequences**:

- `niniBUS` currently passes messages only inside one process.
- The API does not expose transports.
- IPC is deferred to a later milestone.

**Revisit when**:

- V0 and V0.1 are complete.
- The project starts V4 IPC work.

## DD-003 - Use Lanes As The Routing Abstraction

**Status**: accepted

**Decision**: messages travel through numeric lanes identified by `lane_t`.

**Rationale**:

Names such as topic, sensor, diagnostics, command, CAN, or Ethernet carry domain
meaning. `Lane` is intentionally neutral: it is only an independent
communication path. Applications assign meaning to lane IDs.

**Consequences**:

- The bus stays application-agnostic.
- A lane can represent any logical stream chosen by the application.
- Documentation must explain that lane IDs are user-defined.

**Revisit when**:

- The project needs named lanes, typed lanes, or discovery metadata.

## DD-004 - One FIFO Queue Per Lane

**Status**: accepted

**Decision**: each lane owns one FIFO queue of `std::string` messages.

**Rationale**:

- FIFO ordering is easy to reason about.
- Independent queues isolate traffic between lanes.
- This keeps the V0 implementation small.

**Consequences**:

- Messages in the same lane are received in publish order.
- Lanes do not block each other at the data-structure level.
- Multiple receivers on the same lane compete for messages.
- The bus is a work queue, not broadcast pub/sub.

**Alternative considered**: one global queue.

**Rejected because**:

- It creates head-of-line blocking between unrelated message streams.
- It makes dispatching more complex.
- It weakens lane isolation.

**Revisit when**:

- Broadcast semantics become a requirement.
- Per-subscriber queues or cursors are introduced.

## DD-005 - Publish, Receive, Subscribe API

**Status**: current

**Decision**: expose three core operations with explicit publish and receive
status reporting:

```cpp
PublishResult publish(lane_t laneID, std::string message);
ReceiveStatus receive(lane_t laneID, std::string& message);
bool subscribe(lane_t laneID);
```

**Rationale**:

- `publish()` is the familiar operation for adding data to a bus, and now
  returns both publish status and remaining lane credit.
- `receive()` clearly communicates destructive FIFO consumption and returns a
  `ReceiveStatus` so callers can distinguish success, empty lane, and lazy lane
  creation.
- `subscribe()` currently means "ensure this lane exists" and remains a simple
  `bool` operation.

**Consequences**:

- The API is small and direct.
- Publishers can react to `PublishStatus::LaneFull`.
- Publishers can read `PublishResult::Credit` without separately querying lane
  internals.
- `subscribe()` does not currently register callbacks or receiver identity.
- The name `subscribe()` may sound stronger than its current behavior.

**Revisit when**:

- Receiver tracking is added.
- Callback subscriptions are introduced.
- `subscribe()` needs a result enum for consistency.

## DD-006 - Store Lanes By Value

**Status**: accepted

**Decision**: store lanes directly in the bus map:

```cpp
std::unordered_map<uint32_t, Lane> lane_map_;
```

This replaces the earlier two-structure design:

```cpp
std::vector<Lane*> lanes_;
std::unordered_map<uint32_t, uint32_t> lane_map_; // laneID -> vector index
```

**Rationale**:

- The old design required two lookups/steps: find the lane ID in the map, then
  use the stored index to access the vector.
- The old design stored owning raw pointers in the vector, which created manual
  lifetime management risk.
- Keeping the lane object directly in the map removes the separate vector index
  layer.
- Direct map storage saves bookkeeping memory by removing the extra vector and
  index values.
- Direct map storage saves compute by avoiding the map-to-index-to-vector
  indirection.
- Value storage avoids raw owning pointers.
- Lane lifetime is tied to the `niniBUS` object.
- Container cleanup handles lane destruction automatically.
- It removes manual `new`/`delete` ownership problems.

**Consequences**:

- The current implementation has no per-lane ownership leak.
- There is only one authoritative lane registry: `lane_map_`.
- Map lookup returns the lane object directly.
- `Lane` must remain cheaply movable/copyable enough for map storage.
- `unordered_map` still performs dynamic allocation internally.

**Alternative replaced**: `std::vector<Lane*>` plus a `laneID -> vector index`
map.

**Rejected because**:

- It duplicated registry state across two containers.
- The map and vector could become inconsistent.
- It required manual pointer ownership.
- It added avoidable memory and lookup indirection for the current design.

**Revisit when**:

- Memory profiling shows `unordered_map` overhead is too high.
- The project moves toward fixed-size embedded storage.
- `Lane` gains non-copyable resources.

## DD-007 - Lazy Lane Creation

**Status**: accepted

**Decision**: create lanes lazily when they are first published to, subscribed
to, or received from.

**Rationale**:

- Callers do not need a separate setup phase.
- Publishing to a new lane works immediately.
- Receiving from a missing lane prepares the lane for future messages.

**Consequences**:

- Missing-lane receive returns `ReceiveStatus::LazyLaneCreated`.
- `ReceiveStatus::LaneNotFound` is currently defined but not produced.
- Applications that want strict lane registration need extra policy later.

**Revisit when**:

- Strict lane registration is required.
- Missing-lane receive should be an error instead of creating a lane.

## DD-008 - Result Types For Publish And Receive

**Status**: current

**Decision**: `publish()` returns a result struct containing status and credit,
while `receive()` returns an enum status.

```cpp
enum class PublishStatus {
    Ok,
    LaneNotFound,
    LaneFull
};

struct PublishResult {
    uint32_t Credit;
    PublishStatus Status;
};

enum class ReceiveStatus {
    Ok,
    LaneNotFound,
    LaneEmpty,
    LazyLaneCreated
};
```

**Rationale**:

- Explicit statuses describe outcomes better than plain `bool`.
- `publish()` needs to report both success/failure status and remaining lane
  credit.
- `receive()` can distinguish success, empty lane, and lazy lane creation.
- Bounded lanes use `PublishStatus::LaneFull`.

**Consequences**:

- Callers should check the result before using output data.
- Some status values are currently placeholders.
- Documentation must say which results are actually produced today.

**Revisit when**:

- Placeholder enum values remain unused after V0.1.
- `subscribe()` is converted from `bool` to a result enum.

## DD-009 - Single-Threaded For V0

**Status**: accepted

**Decision**: V0 has no internal synchronization.

**Rationale**:

- The current milestone is focused on core behavior.
- Adding mutexes or blocking waits would expand the design surface.
- Thread-safety decisions should be explicit and tested.

**Consequences**:

- Concurrent calls to `publish()`, `receive()`, or `subscribe()` are unsafe.
- The documentation must describe the bus as single-threaded.
- Multi-threading is deferred to V3.

**Revisit when**:

- V3 concurrency starts.
- The example or tests introduce multiple threads.

## DD-010 - Static Library And Separate Example

**Status**: accepted

**Decision**: the root `Makefile` builds only the static library, and
`example/Makefile` builds only the example executable.

**Rationale**:

- The library and example have separate responsibilities.
- The example demonstrates library usage without being part of the library
  build.
- Each `make all` leaves only the final artifact and removes `.o`/`.d` metadata
  files.

**Consequences**:

- Root build output is `libniniBUS.a`.
- Example build output is `example/niniBUS_example`.
- Users build the example from inside `example/`.

**Revisit when**:

- Tests are added.
- Install/package targets are added.
- A higher-level build system is introduced.

## DD-011 - Defer Memory Optimization

**Status**: deferred

**Decision**: do not optimize memory usage during V0.

**Rationale**:

There is not yet measurement data showing that memory is a bottleneck. V0 should
prefer clear implementation over premature compactness.

**Consequences**:

- `std::unordered_map` and `std::deque` are acceptable for now.
- Fixed-size tables, custom allocators, and pool allocators are postponed.
- V2 owns embedded optimization work.

**Revisit when**:

- Memory measurements exist.
- The project defines hard memory limits.
- V2 starts.

## DD-012 - Lane Credit

**Status**: current

**Decision**: every lane exposes remaining publish credit through
`PublishResult`.

Lane credit is derived as:

```text
Credit = Capacity - Queue Size
```

In the current implementation, credit is returned after `publish()`:

```cpp
struct PublishResult {
    uint32_t Credit;
    PublishStatus Status;
};
```

**Rationale**:

- Credit gives producers immediate feedback about remaining lane capacity.
- The idea is inspired by TCP-style flow control.
- Instead of only accepting or rejecting messages, the bus reports how much
  buffer space remains.
- Applications can use credit to adapt publishing rate without inspecting the
  internal lane queue.
- Credit is derived from lane state, not stored as separate state.

**Consequences**:

- Each lane has a capacity.
- `PublishResult::Credit` reports remaining capacity after a publish attempt.
- Publishing to a full lane returns `PublishStatus::LaneFull`.
- Credit currently changes after publish and receive operations, but only
  `publish()` returns the credit value.
- The queue implementation remains hidden from API users.

**Current scope**:

- Credit is returned after `publish()`.
- Credit is derived from `capacity - content.size()`.
- Lane capacity is currently fixed by `MAX_LANE_CAPACITY`.

**Future possibilities**:

- High/low watermarks.
- Adaptive publishing.
- Rate limiting.
- Congestion control.
- Returning updated credit from `receive()`.
- Per-lane capacity configuration.

**Revisit when**:

- Credit needs to be thread-safe.
- Capacity becomes configurable.
- The bus adds blocking publish or receive APIs.
- Back-pressure policy becomes more complex than `LaneFull`.

## DD-013 - Documentation Tracks The Code

**Status**: accepted

**Decision**: README, design notes, milestones, and decision records should be
updated when public behavior or architecture changes.

**Rationale**:

The project is still evolving quickly. Stale documentation creates confusion,
especially when API return types or ownership models change.

**Consequences**:

- API changes should include documentation updates.
- Design documents should describe current behavior, not only intended future
  behavior.
- Milestones should capture deferred ideas instead of letting them leak into the
  current implementation.

**Revisit when**:

- The project adds generated API docs.
- The public API stabilizes enough for formal versioned documentation.

## Open Decisions

- Should `subscribe()` keep returning `bool`, or should it return a dedicated
  result enum?
- Should the destructor print shutdown messages, or should library cleanup be
  quiet by default?
- Should unused enum values be removed until they are implemented?
- Should missing-lane receive create a lane, or should it return a strict
  not-found result?
- Should the example be updated to check `ReceiveStatus` before printing the
  output string?

## Design Philosophy

`niniBUS` should remain:

- Simple.
- Predictable.
- Embedded-friendly.
- Easy to understand.
- Easy to extend.
- Well documented.

Every feature added should answer one question:

Does this make the bus better, or only more complicated?
