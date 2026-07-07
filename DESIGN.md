# niniBUS Design

`niniBUS` is a small in-process message bus. It stores string messages by
numeric lane ID and lets callers publish to, subscribe to, and receive from
those lanes.

The current implementation is intentionally minimal. Each lane owns one FIFO
message queue, and `receive()` removes one message from that queue.

## Source Files

- `niniBUS.h` declares the public API and the internal `Lane` container.
- `niniBUS.cpp` implements lane creation, publishing, subscribing, and receiving.
- `Makefile` builds `niniBUS.cpp` into the static library `libniniBUS.a`.
- `example/main.cpp` demonstrates single-threaded usage.
- `example/Makefile` builds the example executable and links it against
  `../libniniBUS.a`.

## Public API

```cpp
using lane_t = uint32_t;

bool publish(lane_t laneID, std::string message);
bool receive(lane_t laneID, std::string& message);
bool subscribe(lane_t laneID);
```

`lane_t` identifies a logical message lane.

## Core Data Model

### `Lane`

`Lane` stores queued messages for one lane.

Fields:

- `laneID`: the numeric lane identifier.
- `content`: a `std::deque<std::string>` containing pending messages.

`Lane` has a defaulted copy constructor and a deleted assignment operator.
The destructor currently performs no cleanup or logging.

### `niniBUS`

`niniBUS` stores lane ownership in:

```cpp
std::unordered_map<uint32_t, Lane*> lane_map_;
```

The map key is the lane ID. The map value is a raw pointer to the `Lane` object
for that lane.

Older vector/index fields are commented out in the header. The active
implementation no longer uses a vector of lanes or a static lane index.

## Lane Creation

Lanes are created lazily.

A lane can be created by:

- `publish(laneID, message)` when publishing to a missing lane.
- `subscribe(laneID)` when subscribing to a missing lane.
- `receive(laneID, message)` indirectly, when receiving from a missing lane.

Creation flow:

1. Search `lane_map_` for `laneID`.
2. If missing, allocate `new Lane(laneID)`.
3. Insert the pointer into `lane_map_`.
4. Use the stored `Lane*` for later operations.

## Publish Flow

`publish(laneID, message)` appends a message to a lane queue.

Algorithm:

1. Look up `laneID` in `lane_map_`.
2. If missing, create and insert a new `Lane`.
3. Append `message` to `Lane::content`.
4. Return `true`.

Messages are pushed to the back of the deque, so delivery order is FIFO.

Complexity:

- Average `O(1)` lane lookup.
- Average `O(1)` lane insertion.
- `O(1)` deque append.

## Subscribe Flow

`subscribe(laneID)` ensures that a lane exists.

Algorithm:

1. Look up `laneID`.
2. If missing, create and insert a new `Lane`.
3. Return `true`.

The current code does not track subscribers beyond creating the lane. The old
`num_receivers` field is commented out and has no runtime effect.

## Receive Flow

`receive(laneID, message)` tries to remove one queued message from a lane.

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
5. Copy the oldest queued message into `message`.
6. Remove that message from the deque.
7. Return `true`.

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

That creates `libniniBUS.a` and removes generated `.o` and `.d` files.

The example owns its own build:

```bash
cd example
make all
```

That creates `example/niniBUS_example` and removes generated `.o` and `.d`
files from the example folder.

## Ownership And Lifetime

The bus allocates lanes with `new` and stores raw `Lane*` values in
`lane_map_`.

The current destructor prints shutdown messages but does not delete the lanes.
As written, allocated lanes leak when the bus is destroyed.

A safer future design would use:

- `std::unordered_map<lane_t, Lane>` for direct value ownership, or
- `std::unordered_map<lane_t, std::unique_ptr<Lane>>` for pointer ownership.

## Thread Safety

The current implementation has no synchronization.

Concurrent calls to `publish()`, `subscribe()`, or `receive()` can race on:

- `lane_map_`
- each lane's `content` deque

Treat the bus as single-threaded unless a mutex or another synchronization
strategy is added.

## Error Handling

All public methods return `bool`.

- `publish()` returns `true` after appending a message.
- `subscribe()` returns `true` after ensuring the lane exists.
- `receive()` returns `true` only when a message was received.
- `receive()` returns `false` when the lane was missing or the lane was empty.

Because both missing-lane and empty-lane cases return `false`, callers cannot
distinguish those cases without relying on console output.

## Recommended Next Improvements

1. Replace raw `Lane*` ownership with value storage or `std::unique_ptr`.
2. Delete commented-out fields and stale comments once the pointer-map design is
   final.
3. Add mutex protection if the bus will be used from multiple threads.
4. Replace the `bool` receive result with an enum for clearer error reporting.
5. Decide whether the bus should remain a competing-consumer queue or become a
   broadcast pub/sub bus.
