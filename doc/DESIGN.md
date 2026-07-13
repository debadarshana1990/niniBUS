# niniBUS Design

`niniBUS` is a small in-process message bus. It stores string messages by
numeric lane ID and lets callers publish to, subscribe to, and receive from
those lanes.

The current implementation is intentionally compact. Each lane owns one FIFO
message queue, and `receive()` removes one message from that queue.

## Source Files

- `status.h` declares publish/receive statuses and `PublishResult`.
- `Lane.h` declares the `lane_t` class.
- `Lane.cpp` implements lane-local queue behavior.
- `niniFIFO.h` declares and implements the fixed-size FIFO template.
- `niniFIFO.cpp` exists as the FIFO component translation unit; template method
  definitions stay in the header.
- `niniBUS.h` declares the public bus API.
- `niniBUS.cpp` implements lane lookup, lazy lane creation, and delegation to
  `lane_t`.
- `Makefile` builds `niniBUS.cpp`, `Lane.cpp`, and `niniFIFO.cpp` into the
  static library `libniniBUS.a`.
- `example/hello.cpp` demonstrates single-threaded usage and assert-based
  behavior checks.
- `example/Makefile` builds the example executable and links it against
  `../libniniBUS.a`.

## Public API

```cpp
using laneID_t = uint32_t;

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

PublishResult publish(laneID_t laneID, const std::string& message);
ReceiveStatus receive(laneID_t laneID, std::string& message);
bool subscribe(laneID_t laneID);
```

`laneID_t` identifies a logical message lane. `lane_t` is the lane object stored
inside the bus map.

## Core Data Model

### `lane_t`

`lane_t` stores queued messages for one lane and owns lane-local queue behavior.

Private fields:

- `content`: a `niniFIFO_t<std::string, DEFAULT_LANE_CAPACITY>` containing
  pending messages.

`lane_t` does not store its own lane ID. The lane ID is already the key in
`niniBUS::lane_map_`, so storing the same value inside every lane object would
duplicate state.

`lane_t` is default-constructible and copyable. It does not own external
resources. Current lane capacity comes from the FIFO template argument
`DEFAULT_LANE_CAPACITY`, not from a runtime lane constructor argument.

Public functions today:

- `push(message)`: append a message to the lane queue.
- `pop(message)`: remove the oldest queued message into the output string.

Private helper functions:

- `qsize()`: return the current queued message count.
- `getCapacity()`: return `DEFAULT_LANE_CAPACITY`.
- `getCredit()`: return remaining queue capacity.

The queue data and capacity helpers are private. `niniBUS` does not inspect
lane size or capacity directly; it delegates to `lane_t::push()` and
`lane_t::pop()`.

`lane_t::getCredit()` returns remaining queue capacity:

```cpp
DEFAULT_LANE_CAPACITY - content.size()
```

### `niniFIFO_t`

`niniFIFO_t<T, CAPACITY>` is a fixed-size circular FIFO template.

It stores data in:

```cpp
std::array<T, CAPACITY> buffer;
```

Current FIFO state:

- `head`: index of the oldest element.
- `tail`: index where the next element will be written.
- `currSize`: number of currently queued elements.

Current FIFO operations:

- `push_back(message)`: append unless the FIFO is full.
- `front()`: return the oldest element.
- `pop_front()`: remove the oldest element.
- `isEmpty()`, `isFull()`, `size()`, and `getCapacity()`: inspect FIFO state.

Because `niniFIFO_t` is a class template, its method definitions live in
`niniFIFO.h`. The `.cpp` file only includes the header and documents that the
template implementation is header-owned.

### `niniBUS`

`niniBUS` stores lanes by value:

```cpp
std::unordered_map<laneID_t, lane_t> lane_map_;
```

The map key is the lane ID. The map value is the lane object for that lane.
Because lanes are stored by value, the current implementation does not allocate
lanes with `new` and does not need to manually delete lane objects.

`niniBUS` is intentionally thin. It should not know the details of how lane
queues push, pop, enforce capacity, or compute status. Its job is to locate or
create the correct lane and delegate queue behavior to `lane_t`.

## Lane Creation

Lanes are created lazily.

A lane can be created by:

- `publish(laneID, message)` when publishing to a missing lane.
- `subscribe(laneID)` when subscribing to a missing lane.
- `receive(laneID, message)` indirectly, when receiving from a missing lane.

Creation flow:

1. Call `try_emplace(laneID)`.
2. If the lane is missing, construct and insert the lane.
3. Use the returned iterator to access the stored lane.

Passing only `laneID` lets `std::unordered_map` default-construct the stored
`lane_t` directly when the key is missing. The code does not need to create and
pass an explicit temporary `lane_t()`.

Current implementation:

- `publish()` creates a missing lane with the default `lane_t` constructor.
- `subscribe()` also creates missing lanes with the default `lane_t`
  constructor.
- The default lane owns a FIFO whose capacity is `DEFAULT_LANE_CAPACITY`.
- `receive()` creates a missing lane by calling `subscribe()` and returns
  `ReceiveStatus::LazyLaneCreated`.

## Publish Flow

`publish(laneID, message)` appends a message to a lane queue.

Algorithm:

1. Use `try_emplace()` to find or create the lane.
2. Call `lane_t::push(message)` on the stored lane.
3. Return the result from `lane_t::push()`.

Messages are pushed to the tail of the FIFO, so delivery order is FIFO.
Credit is reported after the publish attempt.

`lane_t::push()` owns capacity checks, queue mutation, credit calculation, and
publish status. This keeps `niniBUS::publish()` boring: it only finds or creates
the lane and delegates the lane-local behavior.

Complexity:

- Average `O(1)` lane lookup.
- Average `O(1)` lane insertion.
- `O(1)` FIFO append.

Current publish statuses:

- `PublishStatus::Ok`
- `PublishStatus::LaneFull`

Defined but currently unused publish status:

- `PublishStatus::LaneNotFound`

## Subscribe Flow

`subscribe(laneID)` ensures that a lane exists.

Algorithm:

1. Use `try_emplace()` with `laneID`.
2. If missing, construct and insert a new `lane_t`.
3. Return `true`.

The current code does not track subscribers beyond creating the lane.

## Receive Flow

`receive(laneID, message)` tries to remove one queued message from a lane.

Algorithm:

1. Clear the output `message`.
2. Look up `laneID`.
3. If missing:
   - Print an error message.
   - Call `subscribe(laneID)` so the lane exists for future messages.
   - Return `ReceiveStatus::LazyLaneCreated`.
4. Call `lane_t::pop(message)`.
5. Return the `ReceiveStatus` produced by `lane_t::pop()`.

`lane_t::pop()` owns the empty-queue check and output-message mutation. This keeps
receive behavior localized to the lane.

`receive()` is destructive. Once a message is received, it is no longer
available.

Defined but currently unused receive result:

- `ReceiveStatus::LaneNotFound`

## Delivery Semantics

The bus has one queue per lane.

This means:

- Each message can be received once.
- Multiple receivers on the same lane compete for messages.
- The bus does not broadcast a copy of each message to every subscriber.
- There are no per-subscriber cursors.

This is work-queue behavior, not broadcast pub/sub behavior.

## Build Layout

The root Makefile builds only the library:

```bash
make all
```

That compiles `niniBUS.cpp`, `Lane.cpp`, and `niniFIFO.cpp`, archives them into
`libniniBUS.a`, and removes generated `.o` and `.d` files.

The example owns its own build:

```bash
cd example
make all
```

That creates `example/hello` and removes generated `.o` and `.d`
files from the example folder.

## Ownership And Lifetime

Lanes are stored by value in `lane_map_`, so normal container destruction
releases all lane objects when the bus is destroyed.

`niniBUS` and `lane_t` do not declare custom destructors right now. They do not
own raw pointers, file handles, threads, sockets, or other resources that need
manual cleanup. The compiler-generated destructors are enough:
`std::unordered_map` destroys the stored lanes, and each lane's `niniFIFO_t`
destroys its internal `std::array<std::string, DEFAULT_LANE_CAPACITY>`.

Older shutdown/debug destructor messages were removed because library object
destruction should not print to stdout/stderr as a side effect. If future code
adds a real owned resource, then a custom destructor or RAII wrapper can be
introduced at that time.

## Thread Safety

The current implementation has no synchronization.

Concurrent calls to `publish()`, `subscribe()`, or `receive()` can race on:

- `lane_map_`
- each lane's private `content` FIFO

Treat the bus as single-threaded unless a mutex or another synchronization
strategy is added.

## Error Handling

`publish()` returns a result struct containing both status and lane credit.
`receive()` returns an enum status. This is clearer than a plain boolean because
callers can distinguish full lanes, empty lanes, and lazily created lanes.

Current behavior:

- `publish()` returns `PublishResult{Credit, Status}`.
- `publish()` returns `PublishStatus::Ok` when a message was queued.
- `publish()` returns `PublishStatus::LaneFull` when a lane has no remaining
  credit.
- `subscribe()` returns `true`.
- `receive()` returns `ReceiveStatus::Ok` when a message was received.
- `receive()` returns `ReceiveStatus::LazyLaneCreated` when a missing lane was
  created for future messages.
- `receive()` can return `ReceiveStatus::LaneNotFound` if future subscription
  creation fails.
- `receive()` returns `ReceiveStatus::LaneEmpty` when the lane exists but has no
  queued messages.

## Recommended Next Improvements

1. Decide whether lane capacity should stay fixed at compile time through
   `DEFAULT_LANE_CAPACITY` or become configurable per lane.
2. Decide whether public lane statistics are needed.
3. Remove unused result values or implement the conditions that produce them.
4. Add mutex protection if the bus will be used from multiple threads.
5. Decide whether the bus should remain a competing-consumer queue or become a
   broadcast pub/sub bus.
