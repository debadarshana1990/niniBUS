# niniBUS Design

`niniBUS` is a small in-process message bus. It stores string messages by
numeric lane ID and lets callers publish to, subscribe to, and receive from
those lanes.

The current implementation is intentionally compact. Each lane owns one FIFO
message queue, and `receive()` removes one message from that queue.

## Source Files

- `status.h` declares publish/receive statuses and `PublishResult`.
- `Lane.h` declares the `Lane` class.
- `Lane.cpp` implements lane-local queue behavior.
- `niniBUS.h` declares the public bus API.
- `niniBUS.cpp` implements lane lookup, lazy lane creation, and delegation to
  `Lane`.
- `Makefile` builds `niniBUS.cpp` and `Lane.cpp` into the static library
  `libniniBUS.a`.
- `example/hello.cpp` demonstrates single-threaded usage and assert-based
  behavior checks.
- `example/Makefile` builds the example executable and links it against
  `../libniniBUS.a`.

## Public API

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

`lane_t` identifies a logical message lane.

## Core Data Model

### `Lane`

`Lane` stores queued messages for one lane and owns lane-local queue behavior.

Private fields:

- `capacity`: the maximum number of queued messages for the lane.
- `content`: a `std::deque<std::string>` containing pending messages.

`Lane` does not store its own lane ID. The lane ID is already the key in
`niniBUS::lane_map_`, so storing the same value inside every `Lane` would
duplicate state.

`Lane` can be constructed with a capacity and copied. It does not own external
resources.

Public functions today:

- `push(message)`: append a message to the lane queue.
- `pop(message)`: remove the oldest queued message into the output string.

Private helper functions:

- `qsize()`: return the current queued message count.
- `getCapacity()`: return the lane capacity.
- `getCredit()`: return remaining queue capacity.

The queue data and capacity helpers are private. `niniBUS` does not inspect
lane size or capacity directly; it delegates to `Lane::push()` and
`Lane::pop()`.

`Lane::getCredit()` returns remaining queue capacity:

```cpp
capacity - content.size()
```

### `niniBUS`

`niniBUS` stores lanes by value:

```cpp
std::unordered_map<uint32_t, Lane> lane_map_;
```

The map key is the lane ID. The map value is the `Lane` object for that lane.
Because lanes are stored by value, the current implementation does not allocate
lanes with `new` and does not need to manually delete lane objects.

`niniBUS` is intentionally thin. It should not know the details of how lane
queues push, pop, enforce capacity, or compute status. Its job is to locate or
create the correct lane and delegate queue behavior to `Lane`.

## Lane Creation

Lanes are created lazily.

A lane can be created by:

- `publish(laneID, message)` when publishing to a missing lane.
- `subscribe(laneID)` when subscribing to a missing lane.
- `receive(laneID, message)` indirectly, when receiving from a missing lane.

Creation flow:

1. Search `lane_map_` for `laneID`.
2. If missing, construct a `Lane`.
3. Store the lane in `lane_map_`.
4. Use the stored lane for later operations.

Current implementation:

- `publish()` creates a missing lane with the default `Lane` constructor, so it
  uses `DEFAULT_LANE_CAPACITY`.
- `subscribe()` also creates missing lanes with the default `Lane` constructor.
- `receive()` creates a missing lane by calling `subscribe()` and returns
  `ReceiveStatus::LazyLaneCreated`.

## Publish Flow

`publish(laneID, message)` appends a message to a lane queue.

Algorithm:

1. Look up `laneID` in `lane_map_`.
2. If the lane exists, call `Lane::push(message)` and return its result.
3. If the lane does not exist, construct a new `Lane`, insert it into the map,
   call `Lane::push(message)`, and return success.

Messages are pushed to the back of the deque, so delivery order is FIFO.
Credit is reported after the publish attempt.

`Lane::push()` owns capacity checks, queue mutation, credit calculation, and
publish status. This keeps `niniBUS::publish()` boring: it only finds or creates
the lane and delegates the lane-local behavior.

Complexity:

- Average `O(1)` lane lookup.
- Average `O(1)` lane insertion.
- `O(1)` deque append.

Current publish statuses:

- `PublishStatus::Ok`
- `PublishStatus::LaneFull`

Defined but currently unused publish status:

- `PublishStatus::LaneNotFound`

## Subscribe Flow

`subscribe(laneID)` ensures that a lane exists.

Algorithm:

1. Look up `laneID`.
2. If missing, construct and insert a new `Lane`.
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
4. Call `Lane::pop(message)`.
5. Return the `ReceiveStatus` produced by `Lane::pop()`.

`Lane::pop()` owns the empty-queue check and output-message mutation. This keeps
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

That creates `libniniBUS.a` and removes generated `.o` and `.d` files.

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

The `niniBUS` destructor is currently quiet. Older shutdown messages are left as
comments in the source, but the library does not print during destruction.

## Thread Safety

The current implementation has no synchronization.

Concurrent calls to `publish()`, `subscribe()`, or `receive()` can race on:

- `lane_map_`
- each lane's private `content` deque

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
- `receive()` returns `ReceiveStatus::LaneEmpty` when the lane exists but has no
  queued messages.

## Recommended Next Improvements

1. Decide whether lane capacity should stay fixed at `DEFAULT_LANE_CAPACITY` or
   become configurable per lane.
2. Decide whether public lane statistics are needed.
3. Remove unused result values or implement the conditions that produce them.
4. Add mutex protection if the bus will be used from multiple threads.
5. Decide whether the bus should remain a competing-consumer queue or become a
   broadcast pub/sub bus.
