# niniBUS Design

`niniBUS` is a small in-process message bus. It stores string messages by
numeric lane ID and lets callers publish to, subscribe to, and receive from
those lanes.

The current implementation is intentionally compact. Each lane owns one FIFO
message queue, and `receive()` removes one message from that queue.

## Source Files

- `niniBUS.h` declares the public API, result enums, and internal `Lane`
  container.
- `niniBUS.cpp` implements lane creation, publishing, subscribing, and receiving.
- `Makefile` builds `niniBUS.cpp` into the static library `libniniBUS.a`.
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

`Lane` stores queued messages for one lane.

Fields:

- `laneID`: the numeric lane identifier.
- `capacity`: the maximum number of queued messages for the lane.
- `content`: a `std::deque<std::string>` containing pending messages.

`Lane` can be default-constructed with lane ID `0`, constructed with a specific
lane ID, and copied. It does not own external resources.

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

## Lane Creation

Lanes are created lazily.

A lane can be created by:

- `publish(laneID, message)` when publishing to a missing lane.
- `subscribe(laneID)` when subscribing to a missing lane.
- `receive(laneID, message)` indirectly, when receiving from a missing lane.

Creation flow:

1. Search `lane_map_` for `laneID`.
2. If missing, construct a `Lane` for that ID.
3. Store the lane in `lane_map_`.
4. Use the stored lane for later operations.

## Publish Flow

`publish(laneID, message)` appends a message to a lane queue.

Algorithm:

1. Look up `laneID` in `lane_map_`.
2. If the lane exists, append `message` to that lane's `content`.
3. If the lane exists but `content.size() >= capacity`, return
   `PublishStatus::LaneFull` with zero credit.
4. If the lane does not exist, construct a new `Lane`, insert it into the map,
   append `message`, and return success.
5. Return `PublishResult{Credit, PublishStatus::Ok}` on success.

Messages are pushed to the back of the deque, so delivery order is FIFO.
Credit is reported after the publish attempt.

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
4. If the lane queue is empty:
   - Print an error message.
   - Return `ReceiveStatus::LaneEmpty`.
5. Copy the oldest queued message into `message`.
6. Remove that message from the deque.
7. Return `ReceiveStatus::Ok`.

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

The `niniBUS` destructor currently prints shutdown messages. That makes object
destruction visible in the example, but it may be surprising for users who treat
the bus as a quiet library type.

## Thread Safety

The current implementation has no synchronization.

Concurrent calls to `publish()`, `subscribe()`, or `receive()` can race on:

- `lane_map_`
- each lane's `content` deque

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

1. Decide whether lane capacity should stay fixed at `MAX_LANE_CAPACITY` or
   become configurable per lane.
2. Remove unused result values or implement the conditions that produce them.
3. Add mutex protection if the bus will be used from multiple threads.
4. Decide whether the bus should remain a competing-consumer queue or become a
   broadcast pub/sub bus.
5. Consider making the destructor quiet so the library does not print during
   normal object cleanup.
