# niniBUS Design

`niniBUS` is a small in-process message bus. It groups string messages by a numeric
lane ID and lets callers publish messages to a lane, subscribe to a lane, and
receive queued messages from a lane.

The current implementation is intentionally simple: every lane owns one FIFO
queue, and every `receive()` call removes one message from that queue.

## Source Files

- `niniBUS.h` declares the public API and the internal `Lane` data container.
- `niniBUS.cpp` implements lane creation, publishing, subscribing, and receiving.
- `main.cpp` demonstrates basic single-threaded usage.

## Core Types

### `lane_t`

`lane_t` is an alias for `uint32_t`.

It identifies a logical message lane:

```cpp
using lane_t = uint32_t;
```

### `Lane`

`Lane` stores all queued messages for one lane.

Fields:

- `laneID`: the numeric lane identifier.
- `content`: a `std::deque<std::string>` that stores pending messages.
- `num_receivers`: a receiver count field. It is present in the data model, but
  the current implementation does not consistently maintain or use it for
  delivery behavior.

Construction prints debug output showing the lane ID and receiver count.
Destruction prints debug output for the lane.

Copy construction is defaulted, but assignment is deleted.

### `niniBUS`

`niniBUS` owns the lane registry.

Private fields:

- `std::vector<Lane*> lanes_`
  - Stores pointers to allocated `Lane` objects.
- `static uint32_t lanes_idx_`
  - Monotonically increases when new lanes are created.
  - Used as the index assigned to newly created lanes.
- `std::unordered_map<uint32_t, uint32_t> lane_map_`
  - Maps `laneID` to an index in `lanes_`.

Public API:

```cpp
bool publish(lane_t laneID, std::string message);
bool receive(lane_t laneID, std::string& message);
bool subscribe(lane_t laneID);
```

## Lane Creation

Lanes are created lazily.

A lane can be created by either:

- `publish(laneID, message)` when publishing to a lane for the first time.
- `subscribe(laneID)` when subscribing to a lane for the first time.
- `receive(laneID, message)` indirectly, when receiving from an unknown lane.

Creation flow:

1. Look up `laneID` in `lane_map_`.
2. If the lane does not exist, allocate `new Lane(laneID)`.
3. Store the current `lanes_idx_` in `lane_map_[laneID]`.
4. Increment `lanes_idx_`.
5. Push the new `Lane*` into `lanes_`.

The map and vector must stay synchronized. For every entry in `lane_map_`, the
mapped index is expected to point to the matching `Lane*` in `lanes_`.

## Publish Flow

`publish(laneID, message)` appends a message to a lane queue.

Algorithm:

1. Look up `laneID`.
2. If missing, create a new lane.
3. Resolve the lane index from `lane_map_`.
4. Append `message` to `lanes_[idx]->content`.
5. Return `true`.

Delivery order within a lane is FIFO because messages are pushed to the back of
the deque and received from the front.

Complexity:

- Average `O(1)` lane lookup.
- Amortized `O(1)` lane insertion into the vector.
- `O(1)` message append to the deque.

## Subscribe Flow

`subscribe(laneID)` ensures that a lane exists.

Algorithm:

1. Look up `laneID`.
2. If missing, create a new lane.
3. Increment `num_receivers` only for the newly created lane.
4. Return `true`.

Important current behavior:

- Calling `subscribe()` on an existing lane does not increment `num_receivers`.
- A newly constructed `Lane` starts with `num_receivers == 1`, and
  `subscribe()` increments it again when it creates the lane. That means a lane
  created by `subscribe()` starts at `2`, while a lane created by `publish()`
  starts at `1`.
- `num_receivers` does not affect message retention or routing.

## Receive Flow

`receive(laneID, message)` tries to pop one queued message from a lane.

Algorithm:

1. Clear the output `message`.
2. Look up `laneID`.
3. If missing:
   - Print an error message.
   - Call `subscribe(laneID)` so the lane exists for future messages.
   - Return `false`.
4. If the lane queue is empty:
   - Print an error message.
   - Return `false`.
5. Copy the front queued message into `message`.
6. Pop that message from the queue.
7. Return `true`.

The receive operation is destructive. Once a message is returned by `receive()`,
it is removed from the lane and cannot be received again.

## Message Delivery Semantics

The current bus uses one queue per lane.

That means multiple receivers on the same lane compete for messages:

- Each message can be received once.
- The first receiver to call `receive()` gets the oldest available message.
- There is no broadcast behavior where every receiver gets its own copy.
- There is no per-subscriber cursor or offset.

This design is closer to a work queue than a pub/sub broadcast bus.

## Ownership And Lifetime

`publish()` and `subscribe()` allocate lanes with `new`.

Current lifetime behavior:

- `niniBUS` stores raw `Lane*` pointers.
- `niniBUS::~niniBUS()` prints messages, but does not delete the allocated
  `Lane` objects.
- As written, created lanes leak memory.

A safer design would store lanes by value or use `std::unique_ptr<Lane>` and
release them automatically when the bus is destroyed.

## Static Index Caveat

`lanes_idx_` is static, so it is shared by every `niniBUS` instance.

`lane_map_` and `lanes_` are not static, so they are per-instance.

This combination can break if more than one `niniBUS` object is created. The
second bus may assign a new lane an index greater than zero even though its own
`lanes_` vector is empty, causing later `lanes_[idx]` access to be invalid.

The index should be an instance member, or lane indices should be derived from
`lanes_.size()`.

## Thread Safety

The current implementation has no mutexes or other synchronization.

Concurrent calls to `publish()`, `subscribe()`, or `receive()` can race while
reading or writing:

- `lane_map_`
- `lanes_`
- `lanes_idx_`
- each lane's `content` deque
- each lane's `num_receivers`

The bus should be treated as single-threaded unless synchronization is added.

## Error Handling

All public methods return `bool`.

Current return behavior:

- `publish()` always returns `true` unless the program terminates due to an
  exception or undefined behavior.
- `subscribe()` always returns `true` unless the program terminates due to an
  exception or undefined behavior.
- `receive()` returns `false` when the lane does not exist or when the lane has
  no queued messages.

`receive()` uses the same `false` result for different conditions. Callers cannot
distinguish "lane did not exist" from "lane exists but is empty" without relying
on console output.

## Current Invariants

The implementation depends on these invariants:

- Every lane ID in `lane_map_` maps to a valid index in `lanes_`.
- The `Lane*` at that index belongs to the same lane ID.
- `lanes_idx_` always points to the next unused vector index.
- A message is only removed after it has been copied into the caller-provided
  output string.

Because the data members are private, only `niniBUS` methods can normally
preserve or break these invariants.

## Recommended Next Improvements

The smallest practical improvements are:

1. Replace `std::vector<Lane*>` with `std::vector<std::unique_ptr<Lane>>`, or
   store `Lane` objects directly.
2. Make `lanes_idx_` a non-static member, or remove it and use `lanes_.size()`
   when assigning new lane indices.
3. Make `subscribe()` increment `num_receivers` consistently, or remove
   `num_receivers` until receiver tracking is implemented.
4. Add a mutex around all access to bus state if the bus is used by multiple
   threads.
5. Replace the `bool` receive result with a small enum so callers can distinguish
   success, missing lane, and empty lane.
6. Decide whether the bus should be a competing-consumer queue or a broadcast
   pub/sub bus, then make the data model match that behavior.

