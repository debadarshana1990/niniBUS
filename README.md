# niniBUS

`niniBUS` is a small, single-threaded, in-process message bus built around
named lanes and cursor-based broadcast delivery. Think public transit, except
the buses are strings, the lanes are integers, and nobody has to pretend they
didn't see you standing at the stop.

Each lane owns one bounded `nbus::cfifo<std::string>`. Subscribers do not own
private message queues. Instead, every subscriber has a cursor into the lane's
shared sequence of messages. That lets several subscribers read the same
message independently without duplicating message storage. One message, many
readers, significantly less hoarding.

> [!WARNING]
> `niniBUS` is **not thread-safe yet**. Bring one thread and enjoy the ride.
> Bring two and embrace disappointment, undefined behavior, and a debugging
> session that builds character.

## The “Writers Have Places To Be” Policy

The current policy prioritizes fresh data and keeps writers moving: **when
pressured during a write, niniBUS makes room for the new message by deleting
the oldest retained data.** In other words, when the choice is old data or new
data, niniBUS keeps the new data.

- `publish()` always accepts the message. Rejection is exhausting.
- Publishing to a missing lane lazily creates that lane with
  `DEFAULT_LANE_CAPACITY`. Call `createLane()` first when a specific capacity
  is required.
- When a lane is full, the oldest retained data is reclaimed and the slowest
  subscribers are advanced so the write can continue. The bus waits for no
  cursor.
- A slow subscriber can therefore miss messages. We report the loss; we do not
  manufacture closure.
- The next successful `receive()` reports the number skipped in
  `SkippedMessages`.

This is bounded, write-prioritized broadcast delivery. It is not lossless
delivery and it is not backpressure. If every message is sacred, this is not
your cathedral.

## Receiving Is Not Urban Planning

`receive()` never creates a missing lane and never registers a missing
subscriber. A receive operation may advance an existing subscriber cursor,
but it does not mutate the bus topology. Typos should produce errors, not
infrastructure.

- Missing lane: `ReceiveStatus::NO_CURSOR`
- Missing subscriber: `ReceiveStatus::NO_CURSOR`
- Registered subscriber with nothing pending:
  `ReceiveStatus::NO_PENDING_MESSAGE`

Call `createLane()` and `subscribe()` before receiving. The bus is convenient,
not psychic.

This provides two intentional setup styles:

```cpp
// Convenient default topology
bus.publish(10, "hello");

// Explicit configuration
bus.createLane(20, 64);
bus.subscribe(20, 100);
bus.publish(20, "hello");
```

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

## A Small, Surprisingly Functional Example

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
after registration, not retained history from before registration. Subscribers
do not get flashbacks.

## What Came Back From The Journey?

`PublishResult` contains:

- `Credit`: free retained slots after the write.
- `sequenceID`: logical sequence assigned to the written message. Together
  with the lane ID, it uniquely identifies a message in a `niniBUS` instance.

`ReceiveResult` contains:

- `Status`: success, no pending message, or no cursor.
- `PendingMessages`: messages still pending for that subscriber.
- `sequenceID`: sequence read on success.
- `SkippedMessages`: messages forcibly skipped since that subscriber's
  previous successful read. This is the “you missed a few episodes” counter.

## Backpressure: Mind The Gap

The lane is filling up, the producer is still flooring it, and one subscriber
is reading messages like they are terms and conditions. Something has to give.

`niniBUS` does not grab the brakes. It lights up the dashboard and lets the
application decide whether to pause, slow down, or keep driving. If the
application keeps publishing into a full lane, niniBUS favors fresh data:
it deletes the oldest retained data, advances the slowest subscribers, and
makes room for the new message.

The dashboard has three useful gauges:

- `PublishResult::Credit` is lane-wide. It reports how many retained slots
  remain after the publish. A low value means the lane is close to forcing
  slow subscribers forward. `0` means the successful write filled the last
  free slot; it does **not** mean the write failed. The next write still
  succeeds because niniBUS reclaims old data before storing the new message.
- `ReceiveResult::PendingMessages` is subscriber-specific. It reports how many
  currently available messages that subscriber still has to read after the
  current receive. A growing value means that subscriber should stop admiring
  the scenery and catch up.
- `ReceiveResult::SkippedMessages` confirms that the write-prioritized policy
  already advanced that subscriber. Those old messages have left the station.

For example, a producer can pause, reduce its publishing rate, or reject
upstream work when `Credit` reaches a chosen low-water mark. A consumer can
drain more aggressively when `PendingMessages` crosses a chosen high-water
mark:

```cpp
const PublishResult result = bus.publish(10, payload);
if (result.Credit <= LOW_WATER_MARK) {
    // Apply application-specific backpressure before publishing more.
}

const ReceiveResult received = bus.receive(10, subscriber_id, message);
if (received.PendingMessages >= HIGH_WATER_MARK) {
    // Schedule this subscriber to drain the lane more aggressively.
}
```

These values are feedback for application-managed backpressure, not a
lossless guarantee. If the producer continues publishing, niniBUS continues
accepting messages and slow subscribers may report skipped messages.

`sequenceID` is monotonic and unique only within its lane. Use
`(laneID, sequenceID)` as the message identifier; the same sequence value can
exist in two different lanes.

## Rules Of The Road

- `createLane(id, capacity)` rejects zero capacity.
- Creating an existing lane returns `CreateLaneStatus::LaneExists` and does
  not replace its queue.
- Publishing to a missing lane lazily creates it with
  `DEFAULT_LANE_CAPACITY`; call `createLane()` first to select a custom
  capacity. Convenience when you want it, configuration when you mean it.
- Subscribing to a missing lane returns `SubscribeStatus::LaneNotExist`.
- Duplicate subscription is idempotent and does not reset the cursor.
- Unsubscribing a missing lane or subscriber returns `false`.
- A successful unsubscribe removes that cursor from future delivery and
  reclaim decisions.

No lanes are summoned by `subscribe()` or `receive()`. Only `publish()` has a
permit for spontaneous construction.

## Build It, Then Blame The Tests

```sh
make
cd example
make test
```

The example Makefile builds and runs:

- `niniBUS_test` for bus, lane, subscription, and unsubscribe behavior.
- `cfifo_test` for cursor FIFO behavior and reclamation policy.

Use `make clean` in the repository root and `example/` to remove generated
objects, dependency files, libraries, and test executables. A tiny digital
broom, with no opinions about your architecture.

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

## Embrace Disappointment

- **Single-threaded only.** Thread safety is a future adventure. Today, adding
  threads is an exciting way to generate folklore.
- **In-process only.** The network cannot hurt you because there is no network.
- **Messages are copied strings.** Exotic payloads must wear a string costume.
- **No persistence or durability.** Restart the process and the bus achieves
  perfect work-life separation.
- **No acknowledgement, retry, or transport.** Delivery infrastructure sold
  separately.
- **No lossless mode or publisher backpressure.** Slow readers may miss the
  bus, with an exact count of how many buses they missed.
- **Sequence rollover is not yet handled.** You have roughly `2^64` messages to
  prepare an incident response.
- **Removing a cursor does not promise immediate storage compaction.** Even
  memory needs time to let go.

These are explicit boundaries of the current version, not hidden guarantees.
Small software should be honest about which dragons it has not trained yet.
