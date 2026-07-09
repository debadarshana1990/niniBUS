# niniBUS

`niniBUS` is a small C++ in-process message bus.

It stores string messages in numeric lanes. Callers can publish messages to a
lane, subscribe to a lane, and receive queued messages from a lane.

## API

The public API is declared in `niniBUS.h`:

```cpp
using lane_t = uint32_t;

enum class PublishStatus {
    Ok,
    LaneNotFound,
    LaneFull
};

enum class ReceiveStatus {
    Ok,
    LaneNotFound,
    LaneEmpty,
    LazyLaneCreated
};

struct PublishResult {
    uint32_t Credit;
    PublishStatus Status;
};

PublishResult publish(lane_t laneID, std::string message);
ReceiveStatus receive(lane_t laneID, std::string& message);
bool subscribe(lane_t laneID);
```

## How It Works

- A lane is created lazily when it is first published to or subscribed to.
- Each lane stores messages in a FIFO `std::deque<std::string>`.
- Each lane has a fixed capacity of `MAX_LANE_CAPACITY`.
- `publish()` appends a message to a lane and returns `PublishResult`.
- `PublishResult::Status` says whether publish succeeded or the lane was full.
- `PublishResult::Credit` reports remaining lane capacity after the publish
  attempt.
- `receive()` removes the oldest message from a lane when available.
- `subscribe()` ensures that a lane exists.

Each lane has one queue, so multiple receivers on the same lane compete for
messages. A received message is removed and cannot be received again.

## Publish Results

`publish()` returns a `PublishResult`:

- `Status == PublishStatus::Ok` when the message was queued.
- `Status == PublishStatus::LaneFull` when the lane has no remaining credit.
- `Credit` is the number of messages that can still be accepted by that lane.

`PublishStatus::LaneNotFound` exists in the API, but the current implementation
creates missing lanes lazily instead of returning that value.

## Receive Results

`receive()` returns:

- `ReceiveStatus::Ok` when a message was received.
- `ReceiveStatus::LazyLaneCreated` when the lane did not exist and was created
  for future messages.
- `ReceiveStatus::LaneEmpty` when the lane exists but has no queued messages.

`ReceiveStatus::LaneNotFound` exists in the API, but the current implementation
creates missing lanes lazily instead of returning that value.

## Repository Layout

- `niniBUS.h` - public API, result enums, and `Lane` structure.
- `niniBUS.cpp` - message bus implementation.
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
- Subscribe behavior.
- Lane credit.
- Lane full behavior.

## Important Limitations

- The bus is not thread-safe.
- `subscribe()` only creates a lane; it does not track subscriber count.
- `PublishStatus::LaneNotFound` and `ReceiveStatus::LaneNotFound` are defined
  but not currently produced by the implementation.
- Lane capacity is currently fixed by `MAX_LANE_CAPACITY`.
- The destructor prints shutdown messages, which may be noisy for library users.

See [doc/DESIGN.md](doc/DESIGN.md) and the files in [doc/](doc/) for more detail.
