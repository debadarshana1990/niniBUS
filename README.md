# niniBUS

`niniBUS` is a small C++ in-process message bus.

It stores string messages in numeric lanes. Callers can publish messages to a
lane, subscribe to a lane, and receive queued messages from a lane.

## API

The public API is declared in `niniBUS.h`:

```cpp
using lane_t = uint32_t;

enum class PublishResult {
    Ok,
    LaneNotFound,
    LaneFull
};

enum class ReceiveResult {
    Ok,
    LaneNotFound,
    LaneEmpty,
    LazyLaneCreated
};

PublishResult publish(lane_t laneID, std::string message);
ReceiveResult receive(lane_t laneID, std::string& message);
bool subscribe(lane_t laneID);
```

## How It Works

- A lane is created lazily when it is first published to or subscribed to.
- Each lane stores messages in a FIFO `std::deque<std::string>`.
- `publish()` appends a message to a lane and returns `PublishResult::Ok`.
- `receive()` removes the oldest message from a lane when available.
- `subscribe()` ensures that a lane exists.

Each lane has one queue, so multiple receivers on the same lane compete for
messages. A received message is removed and cannot be received again.

## Receive Results

`receive()` returns:

- `ReceiveResult::Ok` when a message was received.
- `ReceiveResult::LazyLaneCreated` when the lane did not exist and was created
  for future messages.
- `ReceiveResult::LaneEmpty` when the lane exists but has no queued messages.

`ReceiveResult::LaneNotFound` exists in the API, but the current implementation
creates missing lanes lazily instead of returning that value.

## Repository Layout

- `niniBUS.h` - public API, result enums, and `Lane` structure.
- `niniBUS.cpp` - message bus implementation.
- `Makefile` - builds the `niniBUS` static library.
- `example/main.cpp` - small example program.
- `example/Makefile` - builds and runs the example.
- `DESIGN.md` - current architecture and implementation notes.
- `doc/DesignDecisions.md` - design decision log.
- `doc/Milestone.md` - phased roadmap and TODO lists.
- `doc/FutureTopics.md` - deferred ideas and research topics.
- `doc/Learning.md` - implementation lessons and compiler notes.

## Documentation

Start here:

- [DESIGN.md](DESIGN.md) - how the current bus is structured.
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
example/niniBUS_example
```

Generated `.o` and `.d` files are removed automatically.

Run the example:

```bash
cd example
make run
```

Clean the example:

```bash
cd example
make clean
```

## Example Behavior

`example/main.cpp`:

1. Creates a `niniBUS` instance.
2. Subscribes to lanes `1` and `2`.
3. Publishes two messages to lane `1`.
4. Receives from lane `1`.
5. Attempts to receive from lane `2`.
6. Receives from lane `1` again.

Lane `1` has two queued messages, so both receives from lane `1` succeed.
Lane `2` exists but has no queued messages, so receiving from lane `2` returns
`ReceiveResult::LaneEmpty` and leaves the output string empty.

## Important Limitations

- The bus is not thread-safe.
- `subscribe()` only creates a lane; it does not track subscriber count.
- `PublishResult::LaneNotFound`, `PublishResult::LaneFull`, and
  `ReceiveResult::LaneNotFound` are defined but not currently produced by the
  implementation.
- The destructor prints shutdown messages, which may be noisy for library users.

See [DESIGN.md](DESIGN.md) and the files in [doc/](doc/) for more detail.
