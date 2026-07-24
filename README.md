# niniBUS

`niniBUS` is a small, single-threaded, in-process message bus built around
named lanes and cursor-based broadcast delivery.

Each lane owns one bounded `nbus::cfifo<std::string>`. Subscribers do not own
private message queues. Instead, every subscriber has a cursor into the lane's
shared sequence of messages. That lets several subscribers read the same
message independently without duplicating message storage.

## Current Delivery Policy

The current policy prioritizes writers:

- `publish()` always accepts the message.
- If a lane does not exist, `publish()` creates it with
  `DEFAULT_LANE_CAPACITY`.
- When a lane is full, the slowest cursor group is advanced so the write can
  continue.
- A subscriber can therefore miss messages.
- The next successful `receive()` reports the number skipped in
  `SkippedMessages`.

This is bounded, write-prioritized broadcast delivery. It is not lossless
delivery and it is not backpressure.

## Receive Does Not Create Topology

`receive()` never creates a missing lane and never registers a missing
subscriber. A receive operation may advance an existing subscriber cursor,
but it does not mutate the bus topology.

- Missing lane: `ReceiveStatus::NO_CURSOR`
- Missing subscriber: `ReceiveStatus::NO_CURSOR`
- Registered subscriber with nothing pending:
  `ReceiveStatus::NO_PENDING_MESSAGE`

Call `createLane()` and `subscribe()` before receiving.

## Public API

```cpp
niniBUS();

PublishResult publish(laneID_t lane_id, const std::string& message);
ReceiveResult receive(laneID_t lane_id,
                      subscribeID_t subscriber_id,
                      std::string& message);

CreateLaneStatus createLane(laneID_t lane_id, uint32_t capacity);
SubscribeResult subscribe(laneID_t lane_id, subscribeID_t subscriber_id);
bool unsubscribe(laneID_t lane_id, subscribeID_t subscriber_id);
```

## Basic Example

```cpp
#include "niniBUS.h"

#include <cassert>
#include <string>

int main()
{
    niniBUS bus;

    assert(bus.createLane(10, 4) == CreateLaneStatus::Ok);
    assert(bus.subscribe(10, 100).status == SubscribeStatus::Ok);
    assert(bus.subscribe(10, 200).status == SubscribeStatus::Ok);

    const PublishResult sent = bus.publish(10, "hello");

    std::string first;
    std::string second;
    const ReceiveResult a = bus.receive(10, 100, first);
    const ReceiveResult b = bus.receive(10, 200, second);

    assert(a.Status == ReceiveStatus::SUCCESS);
    assert(b.Status == ReceiveStatus::SUCCESS);
    assert(first == "hello");
    assert(second == "hello");
    assert(a.sequenceID == sent.sequenceID);
    assert(b.sequenceID == sent.sequenceID);

    assert(bus.unsubscribe(10, 100));
}
```

A cursor starts at the lane's current tail. It receives messages published
after registration, not retained history from before registration.

## Result Meaning

`PublishResult` contains:

- `Status`: currently `PublishStatus::Ok` on the active write path.
- `Credit`: free retained slots after the write.
- `sequenceID`: logical sequence assigned to the written message.

`ReceiveResult` contains:

- `Status`: success, no pending message, or no cursor.
- `PendingMessages`: messages still pending for that subscriber.
- `sequenceID`: sequence read on success.
- `SkippedMessages`: messages forcibly skipped since that subscriber's
  previous successful read.

`PublishStatus::LaneFull` remains declared for compatibility, but the current
write-prioritized `cfifo` policy reclaims space instead of returning it.

## Lane And Subscription Rules

- `createLane(id, capacity)` rejects zero capacity.
- Creating an existing lane returns `CreateLaneStatus::LaneExists` and does
  not replace its queue.
- Publishing to a missing lane creates it with the default capacity.
- Subscribing to a missing lane returns `SubscribeStatus::LaneNotExist`.
- Duplicate subscription is idempotent and does not reset the cursor.
- Unsubscribing a missing lane or subscriber returns `false`.
- A successful unsubscribe removes that cursor from future delivery and
  reclaim decisions.

## Build And Test

```sh
make
cd example
make test
```

The example Makefile builds and runs:

- `niniBUS_test` for bus, lane, subscription, and unsubscribe behavior.
- `cfifo_test` for cursor FIFO behavior and reclamation policy.

Use `make clean` in the repository root and `example/` to remove generated
objects, dependency files, libraries, and test executables.

## Repository Guide

- `niniBUS.h/.cpp`: public bus and lane lookup.
- `Lane.h/.cpp`: lane wrapper around cursor FIFO.
- `cfifo.h`: header-only `nbus::cfifo<T>`.
- `status.h`: public IDs, statuses, and result records.
- `example/niniBUS_test.cpp`: bus-level functional tests.
- `example/cfifo_test.cpp`: data-structure tests.
- `doc/DESIGN.md`: current architecture and operation flows.
- `doc/DesignDecisions.md`: decisions and consequences.
- `doc/cfifo.md`: complete cursor FIFO contract and reclaim policy.
- `doc/Milestone.md`: implemented and future milestones.
- `doc/Learning.md`: C++ and design lessons.
- `doc/FutureTopics.md`: deliberately deferred work.
- `testReport.md`: current coverage summary.

## Current Limitations

- Single-threaded only.
- In-process only.
- Messages are copied strings.
- No persistence, durability, acknowledgement, retry, or transport.
- No lossless mode or publisher backpressure.
- Sequence rollover is not yet handled.
- Removing a cursor does not promise immediate storage compaction.

These are explicit boundaries of the current version, not hidden guarantees.
