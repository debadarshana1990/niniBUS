# niniBUS Design

`niniBUS` is a small in-process message bus. It stores string messages by
numeric lane ID and lets callers publish to and receive from those lanes.

The current implementation is intentionally compact. Each lane owns one FIFO
message queue, and `receive()` removes one message from that queue.

## Source Files

- `status.h` declares publish, receive, and lane-creation statuses plus their
  result types.
- `Lane.h` declares the `lane_t` class.
- `Lane.cpp` implements lane-local queue behavior.
- `niniFIFO.h` declares and implements the FIFO template.
- `niniBUS.h` declares the public bus API.
- `niniBUS.cpp` implements lane lookup, lazy lane creation, and delegation to
  `lane_t`.
- `Makefile` builds `niniBUS.cpp` and `Lane.cpp` into the static library
  `libniniBUS.a`.
- `example/hello.cpp` demonstrates single-threaded usage and assert-based
  behavior checks.
- `example/Makefile` builds the example executable and links it against
  `../libniniBUS.a`.

## Public API

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

`laneID_t` identifies a logical message lane. `lane_t` is the lane object stored
inside the bus map.

## Core Data Model

### `lane_t`

`lane_t` stores queued messages for one lane and owns lane-local queue behavior.

Private fields:

- `content`: a `niniFIFO<std::string>` containing pending messages.

`lane_t` does not store its own lane ID. The lane ID is already the key in
`niniBUS::lane_map_`, so storing the same value inside every lane object would
duplicate state.

`lane_t` follows the rule of zero: it does not declare a custom destructor,
copy constructor, or copy assignment operator. Its members own their own
lifetimes.

Public functions today:

- `push(message)`: append a message to the lane queue.
- `pop(message)`: remove the oldest queued message into the output string.

Private helper functions:

- `getCredit()`: return remaining queue capacity.
- `getPendingMessage()`: return currently queued messages.

The queue data and credit helper are private. `niniBUS` does not inspect lane
size or capacity directly; it delegates to `lane_t::push()` and `lane_t::pop()`.

`lane_t::getCredit()` returns remaining queue capacity:

```cpp
content.capacity() - content.size()
```

### `niniFIFO`

`niniFIFO<T>` is a circular FIFO template.

It stores data in:

```cpp
std::vector<T> buffer_;
```

Current FIFO state:

- `capacity_`: capacity supplied to the FIFO constructor.
- `head_`: index of the oldest element.
- `tail_`: index where the next element will be written.
- `currSize_`: number of currently queued elements.

Current FIFO operations:

- `push_back(message)`: append unless the FIFO is full, returning
  `FIFOStatus`.
- `pop_front()`: remove the oldest element, returning `FIFOStatus`.
- `front()`: return the oldest element, following the STL-style naming used by
  queue-like containers.
- `isEmpty()`, `isFull()`, `size()`, and `capacity()`: inspect FIFO state.

`std::vector` is used instead of `std::array` because both provide contiguous
storage, but `vector` can support runtime capacity and future growth. The
current constructor initializes `capacity_` first, then initializes `buffer_`
with that capacity in the initializer list.

`DEFAULT_LANE_CAPACITY` is declared with `lane_t`. A default-constructed lane
passes that value to its FIFO; an explicitly created lane passes the requested
capacity instead.

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

A lane can be created by:

- `createLane(laneID, capacity)` for an explicit nonzero buffer capacity.
- `publish(laneID, message)` when publishing to a missing lane.
- `receive(laneID, message)` when receiving from a missing lane.

Lazy creation flow:

1. Call `try_emplace(laneID)`.
2. If the lane is missing, construct and insert the lane.
3. Use the returned iterator to access the stored lane.

Passing only `laneID` lets `std::unordered_map` default-construct the stored
`lane_t` directly when the key is missing. The code does not need to create and
pass an explicit temporary `lane_t()`.

Current implementation:

- `createLane()` rejects zero with `CreateLaneStatus::InvalidCapacity`, then
  calls `try_emplace(laneID, capacity)`. It returns `CreateLaneStatus::Ok` for
  a new lane and `CreateLaneStatus::LaneExists` when
  the lane already exists. Existing state and capacity are not replaced.
- `publish()` creates a missing lane with the default `lane_t` constructor.
- The default lane owns a FIFO whose runtime `capacity_` starts at
  `DEFAULT_LANE_CAPACITY`.
- `receive()` uses `try_emplace()` directly. If the `inserted` flag is true, it
  returns `ReceiveResult{0, ReceiveStatus::LazyLaneCreated}`.

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

## Receive Flow

`receive(laneID, message)` tries to remove one queued message from a lane.

Algorithm:

1. Clear the output `message`.
2. Use `try_emplace(laneID)` to get the lane or create it.
3. If `inserted` is true, return
   `ReceiveResult{0, ReceiveStatus::LazyLaneCreated}`.
4. Call `lane_t::pop(message)`.
5. Return the `ReceiveResult` produced by `lane_t::pop()`.

`lane_t::pop()` owns the empty-queue check, output-message mutation, FIFO pop,
and pending-message count. On success, `PendingMessages` reports the number of
messages left in the lane after the received message is removed.

`receive()` is destructive. Once a message is received, it is no longer
available.

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

That compiles `niniBUS.cpp` and `Lane.cpp`, archives them into `libniniBUS.a`,
and removes generated `.o` and `.d` files.

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

`niniBUS`, `lane_t`, and `niniFIFO` do not declare custom destructors or copy
constructors right now. They do not own raw pointers, file handles, threads,
sockets, or other resources that need manual cleanup. The compiler-generated
special members are enough: `std::unordered_map` destroys the stored lanes, and
each lane's `niniFIFO` destroys its internal `std::vector`.

Older shutdown/debug destructor messages were removed because library object
destruction should not print to stdout/stderr as a side effect. If future code
adds a real owned resource, then a custom destructor or RAII wrapper can be
introduced at that time.

## Thread Safety

The current implementation has no synchronization.

Concurrent calls to `publish()` or `receive()` can race on:

- `lane_map_`
- each lane's private `content` FIFO

Treat the bus as single-threaded unless a mutex or another synchronization
strategy is added.

## Error Handling

`publish()` and `receive()` both return result structs. This is clearer than a
plain boolean because callers can distinguish full lanes, empty lanes, and
lazily created lanes while also reading useful queue state.

Current behavior:

- `publish()` returns `PublishResult{Credit, Status}`.
- `publish()` returns `PublishStatus::Ok` when a message was queued.
- `publish()` returns `PublishStatus::LaneFull` when a lane has no remaining
  credit.
- `receive()` returns `ReceiveResult{PendingMessages, Status}`.
- `receive()` returns `ReceiveStatus::Ok` when a message was received.
- `receive()` reports pending messages after a successful pop.
- `receive()` returns `ReceiveStatus::LazyLaneCreated` and `PendingMessages == 0`
  when a missing lane was created for future messages.
- `receive()` returns `ReceiveStatus::LaneEmpty` and `PendingMessages == 0` when
  the lane exists but has no queued messages.

## Recommended Next Improvements

1. Decide whether public lane statistics are needed.
2. Decide whether a maximum lane capacity is needed.
3. Remove unused result values or implement the conditions that produce them.
4. Add mutex protection if the bus will be used from multiple threads.
5. Decide whether the bus should remain a competing-consumer queue or become a
   broadcast pub/sub bus.
