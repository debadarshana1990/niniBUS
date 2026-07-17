# niniBUS

`niniBUS` is a small C++ in-process message bus.

It stores string messages in numeric lanes. Callers can publish messages to a
lane and receive queued messages from a lane.

Current project phase: V1 - Smarter Bus.

## API

The public API is declared in `niniBUS.h`:

```cpp
using laneID_t = uint32_t;

enum class PublishStatus {
    Ok,
    LaneFull
};

enum class ReceiveStatus {
    Ok,
    LaneEmpty,
    LazyLaneCreated
};

struct PublishResult {
    uint32_t Credit;
    PublishStatus Status;
};

struct ReceiveResult {
    uint32_t PendingMessages;
    ReceiveStatus Status;
};

enum class CreateLaneStatus {
    Ok,
    LaneExists,
    InvalidCapacity
};

PublishResult publish(laneID_t laneID, const std::string& message);
ReceiveResult receive(laneID_t laneID, std::string& message);
CreateLaneStatus createLane(laneID_t laneID, uint32_t capacity);
```

## How It Works

- `createLane()` explicitly creates a lane with a caller-selected capacity.
- A lane is also created lazily with `DEFAULT_LANE_CAPACITY` when it is first
  published to or received from.
- Each lane stores messages in `niniFIFO<std::string>`.
- `lane_t` supplies the default capacity, while `niniFIFO` stores the selected
  capacity in a runtime `capacity_` member.
- FIFO storage uses `std::vector`, which is contiguous like `std::array` while
  leaving room for future growth/configuration.
- The lane ID is stored as the key in the bus map, not inside the `lane_t`
  object.
- `lane_t` keeps its queue internals private and owns push/pop behavior.
- `niniBUS` stays intentionally small: it finds or creates a lane, then
  delegates queue behavior to `lane_t`.
- `publish()` appends a message to a lane and returns `PublishResult`.
- `PublishResult::Status` says whether publish succeeded or the lane was full.
- `PublishResult::Credit` reports remaining lane capacity after the publish
  attempt.
- `receive()` removes the oldest message from a lane when available and returns
  `ReceiveResult`.
- `receive()` creates a missing lane and returns
  `ReceiveStatus::LazyLaneCreated` with `PendingMessages == 0`.

Each lane has one queue, so multiple receivers on the same lane compete for
messages. A received message is removed and cannot be received again.

## Publish Results

`publish()` returns a `PublishResult`:

- `Status == PublishStatus::Ok` when the message was queued.
- `Status == PublishStatus::LaneFull` when the lane has no remaining credit.
- `Credit` is the number of messages that can still be accepted by that lane.

## Explicit Lane Creation

`createLane(laneID, capacity)` creates an empty lane with the requested buffer
capacity:

- `CreateLaneStatus::Ok` means the lane was created.
- `CreateLaneStatus::LaneExists` means the ID was already registered. The
  existing lane, queued messages, and capacity are preserved.
- `CreateLaneStatus::InvalidCapacity` means capacity was zero and no lane was
  created. The smallest valid capacity is one.
- Publishing to an unknown lane remains valid and creates it with
  `DEFAULT_LANE_CAPACITY`.

## Receive Results

`receive()` returns a `ReceiveResult`:

- `Status == ReceiveStatus::Ok` when a message was received.
- `Status == ReceiveStatus::LazyLaneCreated` when the lane did not exist and
  was created for future messages.
- `Status == ReceiveStatus::LaneEmpty` when the lane exists but has no queued
  messages.
- `PendingMessages` reports how many messages remain queued after a successful
  receive.
- `PendingMessages == 0` for lazy-created and empty-lane receive results.

## Repository Layout

- `niniBUS.h` - public bus API and lane registry.
- `niniBUS.cpp` - bus map lookup, lazy lane creation, and delegation.
- `Lane.h` - lane API and lane-local queue state.
- `Lane.cpp` - lane-local push/pop behavior.
- `niniFIFO.h` - header-only FIFO template.
- `status.h` - publish and receive status/result types.
- `Makefile` - builds the `niniBUS` static library.
- `example/hello.cpp` - assert-based example tests.
- `example/Makefile` - builds and runs `hello.cpp`.
- `doc/DESIGN.md` - current architecture and implementation notes.
- `doc/DesignDecisions.md` - design decision log.
- `doc/Milestone.md` - phased roadmap and TODO lists.
- `doc/FutureTopics.md` - deferred ideas and research topics.
- `doc/Learning.md` - implementation lessons and compiler notes.

## Documentation

Start here:

- [doc/DESIGN.md](doc/DESIGN.md) - how the current bus is structured.
- [doc/DesignDecisions.md](doc/DesignDecisions.md) - why key design choices
  were made, including the move from vector/index storage to direct map storage.
- [doc/Milestone.md](doc/Milestone.md) - active and future phase TODO lists.
- [doc/FutureTopics.md](doc/FutureTopics.md) - ideas intentionally parked for
  later milestones.
- [doc/Learning.md](doc/Learning.md) - notes from implementation issues, such
  as `unordered_map::operator[]` behavior.

## Build The Library

From the repository root:

```bash
make all
```

This creates:

```bash
libniniBUS.a
```

Generated `.o` and `.d` files are removed automatically.

Clean the library build:

```bash
make clean
```

## Build The Example

From the example folder:

```bash
cd example
make all
```

This creates:

```bash
example/hello
```

Generated `.o` and `.d` files are removed automatically.

Run the hello example tests:

```bash
cd example
make run
```

You can also use:

```bash
cd example
make test
```

Clean the example:

```bash
cd example
make clean
```

## Hello Test Behavior

`example/hello.cpp` uses `assert()` to check:

- FIFO ordering.
- Multiple lanes do not interfere.
- Receive from an empty lane.
- Publish to a non-existing lane.
- Explicit lane creation with a custom capacity.
- Duplicate creation without replacing the existing lane.
- Default capacity for a lane created by `publish()`.
- Lazy lane creation from `receive()`.
- Lane credit.
- Lane full behavior.
- Receive pending-message counts.
- FIFO wraparound and negative FIFO paths.

## Important Limitations

- The bus is not thread-safe.
- There is no `subscribe()` API; lanes can be created explicitly with
  `createLane()` or lazily by `publish()` and `receive()`.
- Capacity is fixed for the lifetime of a lane. Calling `createLane()` for an
  existing ID does not resize or replace it.
- Lane queue internals are private; callers interact through the bus API rather
  than directly mutating lane queues.
- `niniBUS::publish()` and `niniBUS::receive()` are intentionally thin: they
  find or create a lane and delegate queue behavior to `lane_t::push()` or
  `lane_t::pop()`.

See [doc/DESIGN.md](doc/DESIGN.md) and the files in [doc/](doc/) for more detail.
