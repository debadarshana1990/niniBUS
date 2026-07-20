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

**Decision**: messages travel through numeric lanes identified by `laneID_t`.

**Rationale**:

Names such as topic, sensor, diagnostics, command, CAN, or Ethernet carry domain
meaning. The lane abstraction is intentionally neutral: it is only an independent
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
- The current FIFO is `niniFIFO<std::string>`, a circular buffer backed by
  `std::vector`.

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

## DD-005 - Publish And Receive API

**Status**: current

**Decision**: expose publish, receive, and explicit lane creation:

```cpp
PublishResult publish(laneID_t laneID, const std::string& message);
ReceiveResult receive(laneID_t laneID, std::string& message);
CreateLaneStatus createLane(laneID_t laneID, uint32_t capacity);
```

**Rationale**:

- `publish()` is the familiar operation for adding data to a bus, and now
  returns both publish status and remaining lane credit.
- `receive()` clearly communicates destructive FIFO consumption and returns a
  `ReceiveResult` so callers can distinguish success, empty lane, lazy lane
  creation, and pending messages after a successful pop.
- `createLane()` supports applications that need a known per-lane capacity
  before the first message is published.
- A separate `subscribe()` function is not required right now because
  `receive()` already lazily creates missing lanes.
- Removing `subscribe()` avoids an API that only duplicates lane creation
  behavior without tracking real subscribers.

**Consequences**:

- The API is small and direct.
- Publishers can react to `PublishStatus::LaneFull`.
- Publishers can read `PublishResult::Credit` without separately querying lane
  internals.
- Receivers can read `ReceiveResult::PendingMessages` without separately
  querying lane internals.
- There is no subscription registry, callback registration, or subscriber
  identity tracking in the current API.

**Revisit when**:

- Receiver tracking is added.
- Callback subscriptions are introduced.
- Lane creation needs richer metadata or resize semantics.

## DD-006 - Store Lanes By Value

**Status**: accepted

**Decision**: store lanes directly in the bus map:

```cpp
std::unordered_map<laneID_t, lane_t> lane_map_;
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
- The lane ID lives only as the map key, not as duplicated state inside
  `lane_t`.
- `lane_t` should remain compatible with value storage in the map.
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
- `lane_t` gains non-copyable resources.

## DD-007 - Do Not Store Lane ID Inside Lane

**Status**: accepted

**Decision**: `lane_t` does not store `laneID`; the lane ID is represented by the
key in `niniBUS::lane_map_`.

```cpp
std::unordered_map<laneID_t, lane_t> lane_map_;
```

**Rationale**:

- The map key already identifies the lane.
- Storing the same ID inside `lane_t` duplicates state.
- Removing `lane_t::laneID` saves memory per lane.
- Removing duplicated identity avoids consistency questions between the map key
  and the value object.

**Consequences**:

- `lane_t` is now only responsible for queue state and capacity.
- Code that needs the lane ID should use the map key or the API parameter.
- Debugging/logging for lane IDs should happen at the `niniBUS` layer, where the
  lane ID is available.
- Constructors and call sites must not treat the `lane_t` constructor argument as
  a lane ID. The constructor argument now represents capacity.

**Revisit when**:

- Lanes need self-describing metadata independent of the map.
- Lane objects are moved outside `niniBUS::lane_map_`.
- The bus adds named lanes or richer lane descriptors.
- Lane capacity must change after creation.

## DD-008 - Hide Lane Queue Internals

**Status**: accepted

**Decision**: `lane_t` keeps `content_` and credit calculation private. It
exposes push/pop behavior instead of allowing direct mutation or bus-side queue
inspection.

Current public lane operations:

```cpp
PublishResult push(const std::string& message);
ReceiveResult pop(std::string& message);
```

Current private lane helper:

```cpp
uint32_t credit() const;
```

**Rationale**:

- The lane owns its queue invariants.
- `niniBUS` should ask the lane to push and pop instead of reaching into the
  FIFO directly.
- Size, capacity, and credit calculations belong with the lane because they are
  derived from lane-local queue state.
- This keeps future FIFO changes localized inside `lane_t`.

**Consequences**:

- `niniBUS` uses `push()` and `pop()` instead of mutating the lane queue
  directly.
- `credit()` is a private helper.
- Queue implementation details are less exposed.
- Future bounded FIFO or custom queue work can start inside `lane_t`.

**Revisit when**:

- The bus needs more detailed queue inspection APIs.
- A public stats API is needed.

## DD-009 - Separate Lane Implementation

**Status**: accepted

**Decision**: keep lane behavior in separate `Lane.h` / `Lane.cpp` files.

**Rationale**:

- Lane queue behavior is its own responsibility.
- Separating lane behavior from `niniBUS` keeps the bus implementation small.
- Future queue changes should mostly happen in `Lane.cpp`.
- The bus should not need to understand queue internals to publish or receive.

**Consequences**:

- `Lane.cpp` owns the implementation of `push()` and `pop()`.
- `niniBUS.cpp` owns lane lookup, lazy lane creation, and delegation.
- The root Makefile compiles `niniBUS.cpp` and `Lane.cpp` into
  `libniniBUS.a`.

**Revisit when**:

- More lane policies require additional classes.
- The project introduces multiple lane implementations.

## DD-010 - Keep `niniBUS` Boring

**Status**: accepted

**Decision**: keep `niniBUS::publish()` and `niniBUS::receive()` boring:
find or create the lane, then delegate to `lane_t::push()` or `lane_t::pop()`.

**Rationale**:

- `niniBUS` should not know how a lane implements push/pop behavior.
- Capacity checks, queue mutation, credit calculation, and empty-lane pop
  behavior belong to `lane_t`.
- If push/pop behavior changes later, the change should mostly stay inside
  `Lane.cpp`.
- This keeps the bus focused on routing by lane ID.

**Consequences**:

- `publish()` delegates successful publish behavior to `lane_t::push()`.
- `receive()` delegates pop and empty-queue behavior to `lane_t::pop()`.
- `niniBUS` stays easier to read and reason about.

**Current implementation note**:

- `receive()` already delegates queue behavior to `lane_t::pop()`.
- `publish()` delegates capacity checks, queue mutation, credit calculation, and
  publish status to `lane_t::push()`.

**Revisit when**:

- Lane policies expand beyond simple FIFO.
- The bus starts accumulating queue-specific logic again.

## DD-011 - Lazy Lane Creation

**Status**: accepted

**Decision**: keep lazy lane creation when lanes are first published to or
received from, while also allowing explicit creation through `createLane()`.

**Rationale**:

- Callers do not need a separate setup phase.
- Callers that need a custom capacity can create the lane explicitly.
- Publishing to a new lane works immediately.
- Receiving from a missing lane prepares the lane for future messages.
- `receive()` uses `try_emplace(laneID)` so the returned `inserted` flag tells
  whether the lane was just created.

**Consequences**:

- Missing-lane receive normally returns `ReceiveStatus::LazyLaneCreated`.
- Lazy lanes use `DEFAULT_LANE_CAPACITY`.
- The old `find()` plus separate creation path is avoided.

**Revisit when**:

- Strict lane registration is required.
- Missing-lane receive should be an error instead of creating a lane.

## DD-012 - Result Types For Publish And Receive

**Status**: current

**Decision**: `publish()` and `receive()` return result structs containing a
status plus operation-specific queue state.

```cpp
enum class PublishStatus {
    Ok,
    LaneFull
};

struct PublishResult {
    uint32_t Credit;
    PublishStatus Status;
};

enum class ReceiveStatus {
    Ok,
    LaneEmpty,
    LazyLaneCreated
};

struct ReceiveResult {
    uint32_t PendingMessages;
    ReceiveStatus Status;
};
```

**Rationale**:

- Explicit statuses describe outcomes better than plain `bool`.
- `publish()` needs to report both success/failure status and remaining lane
  credit.
- `receive()` needs to report both receive status and the number of messages
  still pending after a successful pop.
- `receive()` can distinguish success, empty lane, and lazy lane creation.
- Bounded lanes use `PublishStatus::LaneFull`.

**Consequences**:

- Callers should check the result before using output data.
- `PublishResult::Credit` describes remaining write capacity.
- `ReceiveResult::PendingMessages` describes queued messages left after receive.
- Status values are limited to outcomes the implementation currently produces.

**Revisit when**:

- Strict lane registration is added.
- More receive failure modes are introduced.

## DD-013 - Single-Threaded For V0

**Status**: accepted

**Decision**: V0 has no internal synchronization.

**Rationale**:

- The current milestone is focused on core behavior.
- Adding mutexes or blocking waits would expand the design surface.
- Thread-safety decisions should be explicit and tested.

**Consequences**:

- Concurrent calls to `publish()` or `receive()` are unsafe.
- The documentation must describe the bus as single-threaded.
- Multi-threading is deferred to V3.

**Revisit when**:

- V3 concurrency starts.
- The example or tests introduce multiple threads.

## DD-014 - Static Library And Separate Example

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
- Example build output is `example/hello`.
- Users build the example from inside `example/`.

**Revisit when**:

- Tests are added.
- Install/package targets are added.
- A higher-level build system is introduced.

## DD-015 - Defer Memory Optimization

**Status**: deferred

**Decision**: do not optimize memory usage during V0.

**Rationale**:

There is not yet measurement data showing that memory is a bottleneck. V0 should
prefer clear implementation over premature compactness.

**Consequences**:

- `std::unordered_map`, `std::vector`, and the current `niniFIFO` are
  acceptable for now.
- Fixed-size tables, custom allocators, and pool allocators are postponed.
- V2 owns embedded optimization work.

**Revisit when**:

- Memory measurements exist.
- The project defines hard memory limits.
- V2 starts.

## DD-016 - Lane Credit

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
- Credit is derived from `capacity - content_.size()`.
- A lane created by `createLane()` uses the requested capacity. A lane created
  lazily uses `DEFAULT_LANE_CAPACITY`. The FIFO stores the selected value in
  its runtime `capacity_` member.

**Future possibilities**:

- High/low watermarks.
- Adaptive publishing.
- Rate limiting.
- Congestion control.
- Returning updated credit from `receive()`.
- Capacity resizing after lane creation.

**Revisit when**:

- Credit needs to be thread-safe.
- Capacity resizing or validation rules change.
- The bus adds blocking publish or receive APIs.
- Back-pressure policy becomes more complex than `LaneFull`.

## DD-017 - Documentation Tracks The Code

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

- Should missing-lane receive create a lane, or should it return a strict
  not-found result?

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

## DD-018 - Lane Owns Queue Behavior For V1

**Status**: accepted

**Decision**: starting in V1, `lane_t` is the owner of lane-local queue behavior.
`niniBUS::publish()` and `niniBUS::receive()` should stay boring: find or
create the lane, then delegate to `lane_t::push()` or `lane_t::pop()`.

`niniBUS` should not know how a lane stores messages, checks capacity, computes
credit, detects an empty queue, pushes a message, or pops a message. Those
details belong inside the `lane_t` implementation.

**Rationale**:

- `lane_t` now has its own implementation file, so queue behavior has a natural
  home in `Lane.cpp`.
- Push and pop are lane-local operations, not bus-routing operations.
- Keeping push/pop isolated means future queue changes should mostly touch
  `lane_t`, not `niniBUS`.
- `niniBUS` should remain responsible for lane lookup, lazy lane creation, and
  API-level routing by lane ID.
- Capacity, size, credit, and queue content are lane internals. The bus should
  not depend on those details.

**Consequences**:

- `lane_t::push()` owns publish-side lane behavior: capacity checks, queue
  mutation, credit calculation, and publish status.
- `lane_t::pop()` owns receive-side lane behavior: empty-queue checks, output
  message mutation, FIFO removal, pending-message calculation, and receive
  status.
- `niniBUS.cpp` should be intentionally plain and easy to read.
- `lane_t` helper methods such as size, capacity, and credit should be private
  unless there is a clear public API reason to expose them.
- If FIFO storage changes from the current `niniFIFO` to another structure,
  the change should be isolated to `lane_t`.

**V1 rule**:

Do not add queue-specific logic back into `niniBUS::publish()` or
`niniBUS::receive()`. If a change is about how messages are accepted, rejected,
stored, credited, or removed from a lane, make that change in `lane_t`.

**Revisit when**:

- The project introduces multiple lane implementations.
- Public lane statistics become part of the official API.
- A future transport layer needs a different separation between routing and
  storage.

## DD-019 - Use `try_emplace()` For Lazy Lane Creation

**Status**: accepted

**Decision**: use `std::unordered_map::try_emplace()` for both lazy and explicit
lane creation.

Current publish-side pattern:

```cpp
auto [it, inserted] = lane_map_.try_emplace(laneID);
return it->second.push(message);
```

Current receive-side pattern:

```cpp
auto [it, inserted] = lane_map_.try_emplace(laneID);
if (inserted)
{
    return {0, ReceiveStatus::LazyLaneCreated};
}
return it->second.pop(message);
```

Explicit creation passes the requested capacity to the lane constructor:

```cpp
auto [it, inserted] = lane_map_.try_emplace(laneID, capacity);
return inserted ? CreateLaneStatus::Ok : CreateLaneStatus::LaneExists;
```

**Rationale**:

- Lazy lane creation is part of the current bus behavior.
- The bus needs an iterator to the stored lane so it can call `lane_t::push()`.
- `try_emplace()` combines lookup and conditional insertion into one map
  operation.
- In `receive()`, the `inserted` flag directly tells whether lazy lane creation
  happened.
- The older `find()` plus `operator[]` approach can search the map once to check
  for the lane, then search again to insert or access the missing lane.
- `operator[]` also default-inserts a value before assignment, which can create
  extra construction and assignment work.
- `try_emplace(laneID)` creates the mapped lane only when the key is missing,
  default-constructs it in place, and returns the iterator needed for the next
  step.

**Consequences**:

- `publish()` stays short: find or create the lane, then call `push()`.
- `receive()` no longer needs a separate `find()` followed by `subscribe()`.
- Missing-lane creation avoids repeated map lookups.
- The code avoids accidental `operator[]` default insertion in publish paths.
- The `inserted` flag is available if future behavior needs to distinguish a
  newly created lane from an existing one.

**Revisit when**:

- Lazy lane creation is removed.
- Lane construction needs policy arguments beyond capacity.
- The map storage strategy changes away from `std::unordered_map`.

## DD-020 - Follow Rule Of Zero For Simple Owners

**Status**: accepted

**Decision**: do not declare custom destructors or copy constructors for
`niniBUS`, `lane_t`, or `niniFIFO` while they only own standard-library value
members.

**Rationale**:

- `niniBUS` stores lanes by value in `std::unordered_map`.
- `lane_t` stores messages in `niniFIFO<std::string>`.
- The FIFO stores messages in `std::vector`, and these value members clean
  themselves up when their owning object is destroyed.
- There are no raw owning pointers, file handles, threads, sockets, or other
  manual resources that need custom cleanup.
- A destructor that only prints messages or has an empty body does not add
  behavior the library needs today.
- Removing explicit destructors and copy constructors follows the C++ rule of
  zero: let the compiler generate special member functions until the class truly
  owns a resource that needs custom management.

**Consequences**:

- Destroying a `niniBUS` object is quiet.
- Queue contents are released automatically through normal container
  destruction.
- The code has less lifecycle boilerplate.
- If future code adds a real owned resource, the project can introduce a custom
  destructor or a dedicated RAII wrapper then.

**Revisit when**:

- `niniBUS` owns resources outside normal value members.
- Lane storage changes to raw pointers or manually managed memory.
- A future transport layer opens files, sockets, threads, or OS handles.

## DD-021 - Use `niniFIFO` With Runtime Capacity State

**Status**: current

**Decision**: each `lane_t` stores messages in:

```cpp
niniFIFO<std::string> content_;
```

`niniFIFO` is a circular FIFO backed by `std::vector<T>`. Its constructor takes
a capacity and stores it in a runtime `capacity_` member.

**Rationale**:

- The lane chooses either its default capacity or the value supplied through
  `createLane()`, then passes that value to the FIFO.
- `std::vector` provides contiguous storage like `std::array`.
- Unlike `std::array`, `std::vector` supports runtime-selected capacity.
- Keeping `capacity_` as a member moves capacity from a compile-time template
  argument to runtime state.
- `buffer_` is initialized from `capacity_` in the constructor initializer list,
  so the vector starts at the FIFO capacity without a separate `resize()` call
  in the constructor body.
- The lane still exposes only `push()` and `pop()`; the bus does not depend on
  FIFO internals.

**Consequences**:

- `lane_t` is default-constructible because its constructor has a default
  capacity argument.
- `try_emplace(laneID)` can default-construct a missing lane directly in
  `lane_map_`.
- Per-lane runtime capacity is exposed through `createLane()`; lazy creation
  continues to use `DEFAULT_LANE_CAPACITY`.
- `niniFIFO.cpp` is no longer part of the build because the FIFO template is
  implemented in the header.

**Revisit when**:

- Capacity must be resized after creation.
- Memory measurements show the current `std::vector` storage is too large.
- Queue behavior needs dynamic capacity, allocation control, or richer
  statistics.

## DD-022 - Keep FIFO Operations Status-Oriented

**Status**: accepted

**Decision**: `niniFIFO::push_back()` and `niniFIFO::pop_front()` return
`FIFOStatus`.

**Rationale**:

- `push_back()` can fail when the FIFO is full.
- `pop_front()` can fail when the FIFO is empty.
- Returning status keeps FIFO mutation APIs aligned with the rest of the project
  style, where operations report success/failure explicitly.
- `front()` remains an STL-style accessor that returns the current front item.

**Consequences**:

- `lane_t::push()` can map `FIFOStatus::FULL` to `PublishStatus::LaneFull`.
- `lane_t::pop()` can use the FIFO state to map empty queues to
  `ReceiveStatus::LaneEmpty`.
- FIFO callers have one consistent way to see mutation failure.

**Revisit when**:

- FIFO APIs need exceptions instead of status values.
- The project introduces richer error payloads.
- `front()` needs a non-throwing alternative.
